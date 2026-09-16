#pragma once

#include <string>

namespace stormworks_ht {

class HeadTrackingMod;

// Records where opengl32_real.dll lives. No IO, so it is safe in DllMain, and
// it must run there, before any thread can reach InitForwarding.
void SetForwardingDir(const std::wstring& exe_dir);

// Loads opengl32_real.dll (the renamed copy of the system opengl32 placed
// beside the proxy) and resolves the two entry points the proxy exports itself.
// Runs once; later and concurrent calls wait for and return the first result.
// Must run off the loader lock (LoadLibrary in DllMain deadlocks for an early
// static import). Returns false if the real DLL or an export could not be resolved.
bool InitForwarding();

// Publishes the live mod instance to the GL hooks. Pass nullptr to detach
// (injection stops; forwarding continues untouched).
void SetMod(HeadTrackingMod* mod);

}  // namespace stormworks_ht
