// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/native_library.h"
#include "base/win/uwp_exception.h"

namespace base {

NativeLibrary LoadNativeLibrary(const FilePath& library_path,
                                NativeLibraryLoadError* error) {
#if defined(WINUWP)
  UWP_API_ERROR("LoadNativeLibraryWithOptions (which calls Win32 APIs)");
#else
  return LoadNativeLibraryWithOptions(
      library_path, NativeLibraryOptions(), error);
#endif  // defined(WINUWP)
}

}  // namespace base
