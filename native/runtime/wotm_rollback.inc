// ABOUTME: Rollback in the runtime: the game state a snapshot holds (guest RAM in use, scratchpad, CPU context, VU0, host state the game
// ABOUTME: reads back, the sound engine) and WOTM_SYNCTEST=<n>, which goes n frames back at every fight frame and checks the redo.
//
// The snapshot is taken at rtMain's frame boundary (entry of timerWaitForMinUpdateRate, right before wotm_sync::frame), where the game's
// frame is finished, the next one has not started and the guest has nothing parked in a host call: the restored CPU context resumes at
// the same pc. Anything that can't be undone there is a barrier: a stage load (wotm_sync::levelLoad), a frame of a dialog loop (its
// boundary is a sceGsSyncV, wotm_overrides.cpp syncVTapped) and a sound bank loaded or unloaded.
//
// WOTM_SYNCTEST=<n> (1..15; fixed step, offline): GGPO's synctest. At every fight frame after n frames of history the state is saved,
// the one from n frames back is restored, and those frames run again with the same inputs (the input timeline is not part of the
// snapshot: the redo reads what the first run read). Resimulated frames skip the drawing capture, the pacing wait, sound output and input
// sampling (resimulating()). Back at the frame where it started, the state is compared byte by byte with the one saved there; a
// difference names the piece, the guest address or offset and how many bytes differ ([synctest] MISMATCH), and every 300 frames a line
// gives the cost: save, restore, compare, and the redone frames' time per frame.
//   WOTM_SYNCTEST_SKIP=<piece>[,<piece>...]: leaves those pieces out of the snapshot (by name prefix: "sound", "orbit", "fixed-step"...),
//     to see what a missing piece breaks.
//   WOTM_SYNCTEST_REDO_SKIP=<hex address>[,...]: those guest functions return at once in the redone frames that are never shown, to
//     find what a redo can leave out (installRedoSkips).
//   WOTM_SYNCTEST_RAM=full: the whole 64 MB host RAM instead of the parts in use (finds state kept somewhere the snapshot doesn't cover).
namespace wotm_rollback
{
    namespace rb = wotm::rollback;
    using clk = std::chrono::steady_clock;

    rb::State g_state;
    std::unique_ptr<rb::SyncTest> g_test;
    std::vector<rb::Snapshot> g_slots;
    rb::Snapshot g_live;          // the state back at the target, compared with the slot saved there
    uint32_t g_soundBanks = 0;    // the sound engine's bank generation at the last boundary
    bool g_fullRam = false;
    PS2Runtime *g_runtime = nullptr;
    R5900Context *g_ctx = nullptr;   // the context the boundary saw (the fight's thread)

    bool resimulating() { return g_test && g_test->resimulating(); }

    // the frame the game runs after the last boundary; a redone frame before the last one of a redo is never shown
    uint32_t g_frame = 0;
    bool hiddenRedo() { return resimulating() && g_frame + 1u < g_test->target(); }

    // ---- WOTM_SYNCTEST_REDO_SKIP=<guest address>[,...]: those functions return at once in the hidden redone frames, to find what a
    // redo can leave out without changing the state (a function whose work only feeds the drawing). The hook goes on last, over
    // any other hook of the function.
    constexpr int kRedoSkips = 8;
    uint32_t g_redoSkipAddr[kRedoSkips] = {};
    PS2Runtime::RecompiledFunction g_redoSkipOrig[kRedoSkips] = {};
    uint64_t g_redoSkipped[kRedoSkips] = {};

    template <int I> void redoSkippable(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == g_redoSkipAddr[I] && hiddenRedo())
        {
            ++g_redoSkipped[I];
            ctx->pc = GPR_U32(ctx, 31);
            return;
        }
        g_redoSkipOrig[I](rdram, ctx, runtime);
    }

    template <int... I> constexpr std::array<PS2Runtime::RecompiledFunction, sizeof...(I)> redoSkipHooks(std::integer_sequence<int, I...>)
    {
        return {&redoSkippable<I>...};
    }

    void installRedoSkips(PS2Runtime &runtime)
    {
        const char *e = std::getenv("WOTM_SYNCTEST_REDO_SKIP");
        if (!g_test || !e)
            return;
        static constexpr auto hooks = redoSkipHooks(std::make_integer_sequence<int, kRedoSkips>{});
        int n = 0;
        for (const char *p = e; *p && n < kRedoSkips;)
        {
            char *end = nullptr;
            const uint32_t addr = static_cast<uint32_t>(std::strtoul(p, &end, 16));
            if (end == p)
                break;
            if (PS2Runtime::RecompiledFunction f = runtime.lookupFunction(addr))
            {
                g_redoSkipAddr[n] = addr, g_redoSkipOrig[n] = f;
                runtime.registerFunction(addr, hooks[static_cast<size_t>(n)]);
                std::fprintf(stderr, "[synctest] hidden redone frames skip the function at 0x%06x\n", addr);
                ++n;
            }
            else
                std::fprintf(stderr, "[synctest] WOTM_SYNCTEST_REDO_SKIP: no function at 0x%06x\n", addr);
            p = *end == ',' ? end + 1 : end;
        }
    }

    // ---- cost (one line per kReportFrames frames)
    constexpr uint32_t kReportFrames = 300u;
    struct Cost
    {
        double sum = 0.0, max = 0.0;
        void add(double v) { sum += v, max = std::max(max, v); }
    };
    Cost g_save, g_restore, g_compare, g_resim, g_present;
    uint32_t g_reported = 0, g_mismatches = 0, g_mismatchesTotal = 0, g_checks = 0, g_barriers = 0;
    clk::time_point g_restoreEnd, g_presentStart;
    bool g_presentValid = false;

    double ms(clk::time_point a, clk::time_point b) { return std::chrono::duration<double, std::milli>(b - a).count(); }

    void report(uint32_t frame)
    {
        if (++g_reported < kReportFrames)
            return;
        const double n = g_reported, back = g_test->frames();
        std::fprintf(stderr,
                     "[synctest] frame %u, %u frames back, avg/max over %u frames: save %.2f/%.2f ms, restore %.2f/%.2f ms, compare "
                     "%.2f/%.2f ms, resimulated frame %.2f/%.2f ms, present frame %.2f/%.2f ms; snapshot %.1f MB; %u checks, %u "
                     "mismatches (%u in all), %u barriers\n",
                     frame, g_test->frames(), g_reported, g_save.sum / n, g_save.max, g_restore.sum / n, g_restore.max,
                     g_compare.sum / n, g_compare.max, g_resim.sum / n / back, g_resim.max / back, g_present.sum / n, g_present.max,
                     g_slots[0].bytes.size() / 1048576.0, g_checks, g_mismatches, g_mismatchesTotal, g_barriers);
        for (int i = 0; i < kRedoSkips && g_redoSkipAddr[i]; ++i)
        {
            std::fprintf(stderr, "[synctest]   0x%06x skipped %llu times in hidden redone frames\n", g_redoSkipAddr[i],
                         static_cast<unsigned long long>(g_redoSkipped[i]));
            g_redoSkipped[i] = 0;   // per report, like the costs above
        }
        g_save = g_restore = g_compare = g_resim = g_present = Cost{};
        g_reported = g_mismatches = g_checks = 0;
    }

    // Every differing range of every piece (runs of differing bytes, joined across gaps under 64 bytes), up to 24 lines: the first
    // difference is often a consequence; the list shows all the state that went another way
    void logRanges(const rb::Snapshot &original, const rb::Snapshot &redone)
    {
        int lines = 0;
        for (size_t i = 0; i < g_state.size() && lines < 24; ++i)
        {
            const size_t at = i ? original.ends[i - 1] : 0, atB = i ? redone.ends[i - 1] : 0;
            const size_t n = std::min(original.ends[i] - at, redone.ends[i] - atB);
            const uint8_t *a = original.bytes.data() + at, *b = redone.bytes.data() + atB;
            const rb::Piece &p = g_state.piece(i);
            for (size_t k = 0; k < n && lines < 24;)
            {
                if (a[k] == b[k])
                {
                    const size_t step = std::min<size_t>(4096u, n - k);
                    if ((k & 4095u) == 0 && std::memcmp(a + k, b + k, step) == 0)
                        k += step;
                    else
                        ++k;
                    continue;
                }
                size_t end = k + 1, differing = 1;
                for (size_t gap = 0; end < n && gap < 64u; ++end)
                    gap = a[end] == b[end] ? gap + 1 : (++differing, 0u);
                if (p.address != rb::Piece::kNoAddress)
                    std::fprintf(stderr, "[synctest]   %s 0x%08zx-0x%08zx: %zu bytes differ\n", p.name.c_str(), p.address + k, p.address + end,
                                 differing);
                else
                    std::fprintf(stderr, "[synctest]   %s offset 0x%zx-0x%zx: %zu bytes differ\n", p.name.c_str(), k, end, differing);
                ++lines;
                k = end;
            }
        }
    }

    void logSizes(const rb::Snapshot &s)
    {
        std::fprintf(stderr, "[synctest] frame %u, snapshot %zu bytes:", s.frame, s.bytes.size());
        for (size_t i = 0; i < g_state.size(); ++i)
            std::fprintf(stderr, "%s %s %zu", i ? "," : "", g_state.piece(i).name.c_str(), s.ends[i] - (i ? s.ends[i - 1] : 0));
        std::fprintf(stderr, "; %zu stream blob(s)\n", s.blobs.size());
    }

    void logMismatch(const rb::State::Diff &d, const rb::Snapshot &original, const rb::Snapshot &redone)
    {
        ++g_mismatches;
        if (++g_mismatchesTotal > 40u)
            return;
        if (d.piece >= g_state.size())
        {
            std::fprintf(stderr, "[synctest] MISMATCH at frame %u: the shared stream data differs\n", original.frame);
            return;
        }
        const rb::Piece &p = g_state.piece(d.piece);
        const size_t at = d.piece ? original.ends[d.piece - 1] : 0;
        char where[64];
        if (p.address != rb::Piece::kNoAddress)
            std::snprintf(where, sizeof where, "guest 0x%08zx", static_cast<size_t>(p.address) + d.offset);
        else
            std::snprintf(where, sizeof where, "offset 0x%zx", d.offset);
        std::fprintf(stderr, "[synctest] MISMATCH at frame %u (redone from %u): %s, %s, %zu bytes differ; original vs redone:",
                     original.frame, original.frame - g_test->frames(), p.name.c_str(), where, d.bytes);
        const size_t from = d.offset & ~size_t(15), end = std::min(original.ends[d.piece], redone.ends[d.piece]) - at;
        for (size_t k = from; k < from + 32u && k < end; k += 4u)
        {
            uint32_t a, b;
            std::memcpy(&a, original.bytes.data() + at + k, 4);
            std::memcpy(&b, redone.bytes.data() + at + k, 4);
            std::fprintf(stderr, " %08x/%08x", a, b);
        }
        std::fputc('\n', stderr);
        if (g_mismatchesTotal <= 3u)
        {
            logRanges(original, redone);
            std::fprintf(stderr, "[synctest]   guest calls queued so far: interrupt %u, alarm %u, GS callback %u, RPC callback %u, syscall %u, exit %u, "
                         "HLE %u, SIF command %u (last at 0x%x)\n",
                         g_wotmInvocations[0].load(), g_wotmInvocations[1].load(), g_wotmInvocations[2].load(), g_wotmInvocations[3].load(),
                         g_wotmInvocations[4].load(), g_wotmInvocations[5].load(), g_wotmInvocations[6].load(), g_wotmInvocations[7].load(),
                         g_wotmLastInvocationPc.load());
        }
    }

    // ---- WOTM_SYNCTEST_TRACE=<frame>: the guest calls of that frame, first run and redo (wotm_sync's CallRecord); at the comparison,
    // the first call where they part ways (the function running just before it computed the first different value)
    uint32_t g_traceFrame = 0xFFFFFFFFu;
    std::vector<wotm_sync::CallRecord> g_traceRuns[2];   // [0] the first run, [1] the redo
    bool g_traceDone[2] = {};

    void traceCall(const R5900Context *ctx, uint32_t target)
    {
        if (wotm_sync::g_index != g_traceFrame)
            return;
        const int run = resimulating() ? 1 : 0;
        if (g_traceDone[run])
            return;
        wotm_sync::CallRecord r{wotm_sync::g_index, target, GPR_U32(ctx, 31), 0u,
                                wotm_sync::hashBytes(reinterpret_cast<const uint8_t *>(ctx->r), sizeof ctx->r),
                                wotm_sync::hashBytes(reinterpret_cast<const uint8_t *>(ctx->f), sizeof ctx->f),
                                wotm_sync::hashBytes(reinterpret_cast<const uint8_t *>(ctx->vu0_vf), sizeof ctx->vu0_vf)};
        g_traceRuns[run].push_back(r);
    }

    void traceReport()
    {
        const auto &a = g_traceRuns[0], &b = g_traceRuns[1];
        size_t i = 0;
        while (i < a.size() && i < b.size() && a[i].target == b[i].target && a[i].ra == b[i].ra && a[i].gpr == b[i].gpr &&
               a[i].fpr == b[i].fpr && a[i].vu0 == b[i].vu0)
            ++i;
        std::fprintf(stderr, "[synctest] trace of frame %u: %zu calls first run, %zu redo; the first %zu are the same\n", g_traceFrame, a.size(),
                     b.size(), i);
        for (size_t k = i >= 6 ? i - 6 : 0; k < i + 4 && (k < a.size() || k < b.size()); ++k)
        {
            auto line = [](const char *what, const std::vector<wotm_sync::CallRecord> &v, size_t k) {
                if (k >= v.size())
                    return (void)std::fprintf(stderr, "[synctest]   %s #%zu: -\n", what, k);
                std::fprintf(stderr, "[synctest]   %s #%zu: call 0x%08x from 0x%08x gpr %016llx fpr %016llx vu0 %016llx\n", what, k,
                             v[k].target, v[k].ra, static_cast<unsigned long long>(v[k].gpr), static_cast<unsigned long long>(v[k].fpr),
                             static_cast<unsigned long long>(v[k].vu0));
            };
            line("first", a, k);
            line("redo ", b, k);
        }
    }

    // ---- the pieces of the state
    bool skipped(const std::string &name)
    {
        static const std::string skip = [] { const char *e = std::getenv("WOTM_SYNCTEST_SKIP"); return e ? std::string(",") + e + "," : std::string(); }();
        for (size_t at = skip.find(','); at != std::string::npos && at + 1 < skip.size(); at = skip.find(',', at + 1))
        {
            const size_t next = skip.find(',', at + 1);
            const std::string prefix = skip.substr(at + 1, next - at - 1);
            if (!prefix.empty() && name.compare(0, prefix.size(), prefix) == 0)
                return true;
        }
        return false;
    }

    void add(rb::Piece p)
    {
        if (skipped(p.name))
        {
            std::fprintf(stderr, "[synctest] piece left out: %s\n", p.name.c_str());
            return;
        }
        g_state.add(std::move(p));
    }

    // The RAM above 32 MB in use: the stage's and monsters' files from 0x2000000 to the end of the load (wotmCheckLoadEnd), the map
    // creator's reserve and the top-of-RAM slots the port's hooks write (texts, view definitions: the last 4 KB)
    rb::Piece upperRamPiece(uint8_t *rdram)
    {
        auto ranges = [] {
            std::vector<std::pair<uint32_t, uint32_t>> r;
            const uint32_t loadEnd = std::min<uint32_t>((g_lastLoadEnd + 0xFFFu) & ~0xFFFu, wotm::stagemap::kReserve);
            if (loadEnd > kHighBase)
                r.emplace_back(kHighBase, loadEnd);
            r.emplace_back(wotm::stagemap::kReserve, PS2_RAM_SIZE);   // the reserve (4 MB) and the slots above it
            return r;
        };
        return {"upper RAM",
                [rdram, ranges](rb::Writer &w) {
                    for (const auto &[from, to] : ranges())
                        w.pod(from), w.pod(to), w.bytes(rdram + from, to - from);
                },
                [rdram, ranges](rb::Reader &r) {
                    for (size_t n = ranges().size(); n > 0 && r.ok; --n)
                    {
                        const uint32_t from = r.pod<uint32_t>(), to = r.pod<uint32_t>();
                        if (from < kHighBase || to > PS2_RAM_SIZE || to < from)
                            r.ok = false;
                        else
                            r.bytes(rdram + from, to - from);
                    }
                },
                kHighBase};
    }

    template <class T> rb::Piece atomicPiece(std::string name, std::atomic<T> &a)
    {
        return {std::move(name), [&a](rb::Writer &w) { w.pod(a.load()); }, [&a](rb::Reader &r) { a.store(r.pod<T>()); }};
    }

    rb::Piece fastClipsPiece()
    {
        return {"modifiers: fast clips",
                [](rb::Writer &w) {
                    std::vector<std::pair<uint32_t, float>> v(wotm_modifiers::g_fastClips.begin(), wotm_modifiers::g_fastClips.end());
                    std::sort(v.begin(), v.end());
                    w.pod(static_cast<uint32_t>(v.size()));
                    for (const auto &[k, f] : v)
                        w.pod(k), w.pod(f);
                },
                [](rb::Reader &r) {
                    wotm_modifiers::g_fastClips.clear();
                    for (uint32_t n = r.pod<uint32_t>(); n > 0 && r.ok; --n)
                    {
                        const uint32_t k = r.pod<uint32_t>();
                        wotm_modifiers::g_fastClips[k] = r.pod<float>();
                    }
                }};
    }

    void addPieces(uint8_t *rdram, R5900Context *ctx)
    {
        PS2Memory &mem = g_runtime->memory();
        if (g_fullRam)
            add(rb::memoryPiece("RAM (64 MB)", rdram, PS2_RAM_SIZE, 0u));
        else
        {
            add(rb::memoryPiece("guest RAM", rdram, PS2_GUEST_RAM_SIZE, 0u));
            add(upperRamPiece(rdram));
        }
        add(rb::memoryPiece("scratchpad", mem.getScratchpad(), PS2_SCRATCHPAD_SIZE, PS2_SCRATCHPAD_BASE));
        add(rb::memoryPiece("CPU context", ctx, sizeof *ctx));
        add(rb::memoryPiece("VU0 data", mem.getVU0Data(), PS2_VU0_DATA_SIZE, PS2_VU0_DATA_BASE));
        for (const ps2_stubs::HostStateBlock &b : ps2_stubs::hostStateBlocks())
            add(rb::memoryPiece(std::string("runtime: ") + b.name, b.data, b.size));
        add(rb::memoryPiece("fixed-step: virtual clock", &wotm_sync::g_virtualTics, sizeof wotm_sync::g_virtualTics));
        add(rb::memoryPiece("fixed-step: clock calls", &wotm_sync::g_ticCalls, sizeof wotm_sync::g_ticCalls));
        add(rb::memoryPiece("fixed-step: vsync parity", &wotm_sync::g_syncCalls, sizeof wotm_sync::g_syncCalls));
        add(rb::memoryPiece("fixed-step: frame index", &wotm_sync::g_index, sizeof wotm_sync::g_index));
        add(rb::memoryPiece("orbit camera: cameras", orbit::g_cam, sizeof orbit::g_cam));
        add(rb::memoryPiece("orbit camera: inputs", orbit::g_in, sizeof orbit::g_in));
        add(rb::memoryPiece("modifiers: gravity", &wotm_modifiers::g_gravityWritten, sizeof wotm_modifiers::g_gravityWritten));
        add(fastClipsPiece());
        add(atomicPiece("intro skip armed", wotm_training::g_introArmed));
        add(wotmSoundRollbackPiece());
        std::fprintf(stderr, "[synctest] snapshot: %zu pieces:", g_state.size());
        for (size_t i = 0; i < g_state.size(); ++i)
            std::fprintf(stderr, "%s %s", i ? "," : "", g_state.piece(i).name.c_str());
        std::fputc('\n', stderr);
        // the guest threads besides the fight's: their contexts and the kernel objects they wait on are not in the snapshot
        const EeKernelSnapshot k = g_runtime->eeScheduler().snapshot();
        std::fprintf(stderr, "[synctest] guest threads (running %d):", k.runningThreadId);
        for (const EeThreadSnapshot &t : k.threads)
            std::fprintf(stderr, " {id %d entry 0x%x pc 0x%x status %d wait %d/%d}", t.id, t.entry, t.pc, static_cast<int>(t.status),
                         static_cast<int>(t.waitReason), t.waitId);
        std::fprintf(stderr, "; %zu semaphores, %zu event flags\n", k.semaphores.size(), k.eventFlags.size());
    }

    void init(PS2Runtime &runtime)
    {
        const char *e = std::getenv("WOTM_SYNCTEST");
        if (!e || std::atoi(e) <= 0)   // unset or 0: off
            return;
        if (!wotm_sync::g_fixedStep || wotm_sync::g_net)
        {
            std::fprintf(stderr, "[synctest] WOTM_SYNCTEST needs WOTM_FIXEDSTEP and no WOTM_NET: off\n");
            return;
        }
        g_runtime = &runtime;
        g_test = std::make_unique<rb::SyncTest>(static_cast<uint32_t>(std::clamp(std::atoi(e), 1, 15)));
        g_slots.resize(g_test->slots());
        const char *ram = std::getenv("WOTM_SYNCTEST_RAM");
        g_fullRam = ram && std::strcmp(ram, "full") == 0;
        std::fprintf(stderr, "[synctest] on: %u frame(s) back at every fight frame%s\n", g_test->frames(), g_fullRam ? ", whole 64 MB RAM" : "");
        if (const char *t = std::getenv("WOTM_SYNCTEST_TRACE"); t && *t)
        {
            g_traceFrame = static_cast<uint32_t>(std::atol(t));
            g_wotmCallTrace = &traceCall;
        }
    }

    // A point the state can't be rolled back across (a stage load, a dialog loop's frame)
    void barrier(const char *why)
    {
        if (!g_test)
            return;
        ++g_barriers;
        if (g_test->barrier())
            std::fprintf(stderr, "[synctest] MISMATCH: %s in the middle of a resimulation (the original frames had none there)\n", why);
        g_presentValid = false;
    }

    // rtMain's frame boundary (waitMinRateByVblanks, right before wotm_sync::frame), in a fight
    void boundary(uint8_t *rdram, R5900Context *ctx)
    {
        if (!g_test || !wotm_sync::g_levelLoaded)
            return;
        // timerWaitForMinUpdateRate is also called by the menus (uiMain) and the shell's loop: only rtMain's call is a fight frame, and a
        // snapshot restored from another caller would resume on another host call chain
        if (GPR_U32(ctx, 31) - kFightLoop >= kFightLoopSize)
            return barrier("frame outside the fight loop");
        if (g_state.size() == 0)
        {
            g_ctx = ctx;
            addPieces(rdram, ctx);
        }
        if (ctx != g_ctx)
        {
            std::fprintf(stderr, "[synctest] the boundary runs on another guest thread's context: barrier\n");
            g_ctx = ctx;
            g_state = rb::State{};   // the "CPU context" piece points at the context it was built with
            addPieces(rdram, ctx);
            barrier("thread change");
            return;
        }
        if (const uint32_t banks = wotmSoundBankGeneration(); banks != g_soundBanks)
        {
            g_soundBanks = banks;
            barrier("sound bank change");
        }
        const uint32_t frame = wotm_sync::g_index;
        g_frame = frame;
        const auto t0 = clk::now();
        switch (g_test->boundary(frame))
        {
        case rb::SyncTest::Step::Resimulated:
            return;
        case rb::SyncTest::Step::Overshot:
            ++g_mismatches, ++g_mismatchesTotal;
            std::fprintf(stderr, "[synctest] MISMATCH: the redo reached frame %u without passing frame %u, where it started\n", frame,
                         g_test->target());
            break;
        case rb::SyncTest::Step::Compare:
        {
            g_resim.add(ms(g_restoreEnd, t0));
            if (frame == g_traceFrame && !g_traceDone[1])
            {
                g_traceDone[1] = true;
                traceReport();
            }
            const rb::Snapshot &original = g_slots[g_test->slot(frame)];
            g_state.save(g_live, frame);
            const rb::State::Diff d = g_state.compare(original, g_live);
            ++g_checks;
            if (!d.equal)
                logMismatch(d, original, g_live);
            const auto t1 = clk::now();
            g_compare.add(ms(t0, t1));
            report(frame);
            g_presentStart = t1;
            g_presentValid = true;
            return;
        }
        case rb::SyncTest::Step::Save:
        case rb::SyncTest::Step::SaveAndBack:
            break;
        }
        if (g_presentValid)
            g_present.add(ms(g_presentStart, t0) - g_paceWaitMs);
        if (frame == g_traceFrame)
            g_traceDone[0] = true;
        g_state.save(g_slots[g_test->slot(frame)], frame);
        const auto t1 = clk::now();
        g_save.add(ms(t0, t1));
        if (frame % 3000u == 0u)   // the pieces' sizes now and then (the sound engine's and the upper RAM's vary)
            logSizes(g_slots[g_test->slot(frame)]);
        if (!g_test->resimulating())
        {
            g_presentStart = t1;
            g_presentValid = true;
            return;
        }
        if (!g_state.load(g_slots[g_test->slot(g_test->back())]))
        {
            // some pieces may be restored and others not: the game can't go on from that
            std::fprintf(stderr, "[synctest] MISMATCH: the snapshot of frame %u can't be restored: the game stops\n", g_test->back());
            g_runtime->requestStop();
            g_test.reset();
            return;
        }
        g_frame = g_test->back();
        g_restoreEnd = clk::now();
        g_restore.add(ms(t1, g_restoreEnd));
        g_presentValid = false;
    }
}
