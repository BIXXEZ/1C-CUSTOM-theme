#pragma once

// ============================================================
// RESOURCE IDS
//
// Shared between DarkModLauncher.rc and DarkModLauncher.cpp.
// ============================================================

//
// The whole DarkModDLL.dll, stored verbatim as RCDATA. The
// launcher writes it back out to disk on startup, because
// injection needs a real file: CreateRemoteThread(LoadLibraryW)
// takes a path, and CallRemoteExport finds the module in the
// target by name through Module32NextW.
//
#define IDR_DARKMOD_DLL 101
