// ABOUTME: Tells a deliberate Enter from the Enter that confirms an input-method (IME) conversion, so a chat line or lobby code
// ABOUTME: is not sent while the player is still composing text.

// Safari delivers the confirming Enter after composition has ended (isComposing already false) with keyCode 229
export function isSubmitEnter(e) {
  return e.key === 'Enter' && !e.isComposing && e.keyCode !== 229;
}
