#include <Windows.h>

#include <string>

#include "v8.h"
#include "libplatform/libplatform.h"

#include "main.h"

int __stdcall AltWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {

  char module_name[1024];
  DWORD result = GetModuleFileNameA(NULL, module_name, 1024);
  
  if (result == 0) {
    ::OutputDebugStringA("Failed to get the module filename");
    return -1;
  }

  // Initialize V8. Below code taken from the "hello-world.cc" sample in V8.
  v8::V8::InitializeICUDefaultLocation(module_name);
  v8::V8::InitializeExternalStartupData(module_name);
  std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  v8::V8::Initialize();

  // Create a new Isolate and make it the current one.
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  if (!isolate) {
    OutputDebugStringA("Failed to create the isolate");
    return -1;
  }

  {
    v8::Isolate::Scope isolate_scope(isolate);

    // Create a stack-allocated handle scope.
    v8::HandleScope handle_scope(isolate);

    // Create a new context.
    v8::Local<v8::Context> context = v8::Context::New(isolate);

    // Enter the context for compiling and running the hello world script.
    v8::Context::Scope context_scope(context);

    {
      // Create a string containing the JavaScript source code.
      v8::Local<v8::String> source =
          v8::String::NewFromUtf8Literal(isolate, cube3d_js);

      // Compile the source code.
      v8::Local<v8::Script> script =
          v8::Script::Compile(context, source).ToLocalChecked();

      // Run the script to get the result.
      v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();

      // Convert the result to an UTF8 string and print it.
      v8::String::Utf8Value utf8(isolate, result);
      ::OutputDebugStringA(*utf8);
    }
  }

  // Dispose the isolate and tear down V8.
  isolate->Dispose();
  v8::V8::Dispose();
  v8::V8::ShutdownPlatform();
  delete create_params.array_buffer_allocator;

  std::wstring cmd_line{GetCommandLineW()};
  std::wstring render_arg{L"--renderer"};
  if (cmd_line.find(render_arg) != std::wstring::npos) {
    //__debugbreak();
    return 42;
  }

  return 0;
}