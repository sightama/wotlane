// ABOUTME: Time source for the PS2X_PAD timed pad script: seconds counted from the first reading instead of raylib's GetTime().
// ABOUTME: Pure; used by wotm_pad.inc in the browser build, where GetTime() is a wall-clock time. Tested in native/tests.
#pragma once

namespace wotm::padclock
{
// The browser's GetTime() returns seconds since the Unix epoch, so script steps like "1:0.5:up" compared to it never fire.
// seconds(raw) counts from the first raw time it is given.
class SinceFirstCall
{
public:
    double seconds(double raw)
    {
        if (!started_)
        {
            started_ = true;
            origin_ = raw;
        }
        return raw - origin_;
    }

private:
    bool started_ = false;
    double origin_ = 0.0;
};
}
