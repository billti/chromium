/*
A UWP app built directly against the WRL library.

For WRL details  https://docs.microsoft.com/en-us/cpp/cppcx/wrl/wrl-reference

Code structure
https://docs.microsoft.com/en-us/archive/msdn-magazine/2013/august/windows-with-c-the-windows-runtime-application-model

Useful reference
https://github.com/Medium/phantomjs-1/blob/master/src/qt/qtbase/src/winmain/qtmain_winrt.cpp

See also code at https://hg.mozilla.org/mozilla-central/rev/5f0882ee58c0
*/

#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY != WINAPI_FAMILY_APP)
#error This app is should be compiled as a Windows Universal app
#endif

#define GL_GLEXT_PROTOTYPES

#include <assert.h>
#include <memory>

#include <Windows.h>

#include <wrl.h>
#include <wrl/wrappers/corewrappers.h>

#include <Windows.ApplicationModel.Activation.h>
#include <Windows.ApplicationModel.Core.h>
#include <Windows.Foundation.Collections.h>
#include <Windows.UI.Core.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglext_angle.h>
#include <EGL/eglplatform.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <angle_windowsstore.h>

#include "simple-renderer.h"

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::ApplicationModel::Core;
using namespace ABI::Windows::ApplicationModel::Activation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::UI::Core;

using namespace CoreAppAngle;

typedef ITypedEventHandler<CoreApplicationView*, IActivatedEventArgs*>
    ActivatedEventHandler;
typedef ITypedEventHandler<CoreWindow*, VisibilityChangedEventArgs*>
    VisibilityChangedEventHandler;
typedef ITypedEventHandler<CoreWindow*, CoreWindowEventArgs*>
    ClosedEventHandler;

inline void CheckHR(HRESULT hr, char const* msg) {
  if (FAILED(hr))
    throw std::exception(msg);
}

class MyApp : public RuntimeClass<IFrameworkView, IFrameworkViewSource> {
 public:
  MyApp()
      : mWindowClosed(false),
        mWindowVisible(true),
        mEglDisplay(EGL_NO_DISPLAY),
        mEglContext(EGL_NO_CONTEXT),
        mEglSurface(EGL_NO_SURFACE) {}

  STDMETHODIMP CreateView(IFrameworkView** viewProvider) {
    *viewProvider = static_cast<IFrameworkView*>(this);
    return S_OK;
  }

  STDMETHODIMP Initialize(ICoreApplicationView* applicationView) {
    return applicationView->add_Activated(
        Callback<ActivatedEventHandler>([this](ICoreApplicationView*,
                                               IActivatedEventArgs*) {
          return this->mCoreWindow->Activate();
        }).Get(),
        &mActivatedToken);
  }

  STDMETHODIMP SetWindow(ICoreWindow* window) {
    mCoreWindow = window;

    window->add_VisibilityChanged(
        Callback<VisibilityChangedEventHandler>(
            [this](ICoreWindow* window, IVisibilityChangedEventArgs* args) {
              args->get_Visible(&this->mWindowVisible);
              return S_OK;
            })
            .Get(),
        &mVisibilityChangedToken);

    window->add_Closed(
        Callback<ClosedEventHandler>([this](ICoreWindow* window,
                                            ICoreWindowEventArgs* args) {
          this->mWindowClosed = true;
          return S_OK;
        }).Get(),
        &mClosedToken);

    InitializeEGL(window);

    return S_OK;
  }

  STDMETHODIMP Load(HSTRING entryPoint) {
    RecreateRenderer();
    return S_OK;
  }

  STDMETHODIMP Run(void) {
    ComPtr<ICoreDispatcher> coreDispatcher;
    CheckHR(mCoreWindow->get_Dispatcher(coreDispatcher.GetAddressOf()),
            "Failed to get dispatcher");

    while (!mWindowClosed) {
      if (mWindowVisible) {
        coreDispatcher->ProcessEvents(
            CoreProcessEventsOption_ProcessAllIfPresent);

        EGLint panelWidth = 0;
        EGLint panelHeight = 0;
        eglQuerySurface(mEglDisplay, mEglSurface, EGL_WIDTH, &panelWidth);
        eglQuerySurface(mEglDisplay, mEglSurface, EGL_HEIGHT, &panelHeight);

        // Logic to update the scene could go here
        mCubeRenderer->UpdateWindowSize(panelWidth, panelHeight);
        mCubeRenderer->Draw();

        // The call to eglSwapBuffers might not be successful (e.g. due to
        // Device Lost) If the call fails, then we must reinitialize EGL and the
        // GL resources.
        if (eglSwapBuffers(mEglDisplay, mEglSurface) != GL_TRUE) {
          mCubeRenderer.reset(nullptr);
          CleanupEGL();

          InitializeEGL(mCoreWindow);
          RecreateRenderer();
        }
      } else {
        coreDispatcher->ProcessEvents(
            CoreProcessEventsOption_ProcessOneAndAllPending);
      }
    }

    CleanupEGL();
    return S_OK;
  }

  STDMETHODIMP Uninitialize(void) { return S_OK; }

 private:
  void RecreateRenderer() {
    if (!mCubeRenderer) {
      mCubeRenderer.reset(new SimpleRenderer());
    }
  }

  void InitializeEGL(ICoreWindow* window) {
    const EGLint configAttributes[] = {EGL_RED_SIZE,   8, EGL_GREEN_SIZE,   8,
                                       EGL_BLUE_SIZE,  8, EGL_ALPHA_SIZE,   8,
                                       EGL_DEPTH_SIZE, 8, EGL_STENCIL_SIZE, 8,
                                       EGL_NONE};

    const EGLint contextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3,
                                        EGL_NONE};

    const EGLint surfaceAttributes[] = {
        // EGL_ANGLE_SURFACE_RENDER_TO_BACK_BUFFER is part of the same
        // optimization as EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER (see
        // above). If you have compilation issues with it then please update
        // your Visual Studio templates.
        /*EGL_ANGLE_SURFACE_RENDER_TO_BACK_BUFFER, EGL_TRUE, */
        EGL_NONE};

    const EGLint defaultDisplayAttributes[] = {
      // These are the default display attributes, used to request ANGLE's D3D11
      // renderer. eglInitialize will only succeed with these attributes if the
      // hardware supports D3D11 Feature Level 10_0+.
      EGL_PLATFORM_ANGLE_TYPE_ANGLE,
      EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,

#if defined(EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER)
      // EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER is an optimization that
      // can have large performance benefits on mobile devices. Its syntax is
      // subject to change, though. Please update your Visual Studio templates
      // if you experience compilation issues with it.
      EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER,
      EGL_TRUE,
#endif

      // EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE is an option that
      // enables ANGLE to automatically call the IDXGIDevice3::Trim method on
      // behalf of the application when it gets suspended. Calling
      // IDXGIDevice3::Trim when an application is suspended is a Windows Store
      // application certification requirement.
      EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE,
      EGL_TRUE,
      EGL_NONE,
    };

    EGLConfig config = NULL;

    // eglGetPlatformDisplayEXT is an alternative to eglGetDisplay. It allows us
    // to pass in display attributes, used to configure D3D11.
    PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT =
        reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
            eglGetProcAddress("eglGetPlatformDisplayEXT"));
    if (!eglGetPlatformDisplayEXT) {
      throw std::exception("eglGetPlatformDisplayEXT failed");
    }

    //
    // To initialize the display, we make three sets of calls to
    // eglGetPlatformDisplayEXT and eglInitialize, with varying parameters
    // passed to eglGetPlatformDisplayEXT: 1) The first calls uses
    // "defaultDisplayAttributes" as a parameter. This corresponds to D3D11
    // Feature Level 10_0+. 2) If eglInitialize fails for step 1 (e.g. because
    // 10_0+ isn't supported by the default GPU), then we try again
    //    using "fl9_3DisplayAttributes". This corresponds to D3D11 Feature
    //    Level 9_3.
    // 3) If eglInitialize fails for step 2 (e.g. because 9_3+ isn't supported
    // by the default GPU), then we try again
    //    using "warpDisplayAttributes".  This corresponds to D3D11 Feature
    //    Level 11_0 on WARP, a D3D11 software rasterizer.
    //

    // This tries to initialize EGL to D3D11 Feature Level 10_0+. See above
    // comment for details.
    mEglDisplay =
        eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY,
                                 defaultDisplayAttributes);
    if (mEglDisplay == EGL_NO_DISPLAY) {
      throw std::exception("eglGetPlatformDisplayETX returned EGL_NO_DISPLAY");
    }

    if (eglInitialize(mEglDisplay, NULL, NULL) == EGL_FALSE) {
      throw std::exception("eglInitialize failed");
    }

    EGLint numConfigs = 0;
    if ((eglChooseConfig(mEglDisplay, configAttributes, &config, 1,
                         &numConfigs) == EGL_FALSE) ||
        (numConfigs == 0)) {
      throw std::exception("eglChooseConfig failed");
    }

    // Create a PropertySet and initialize with the EGLNativeWindowType.
    ComPtr<IPropertySet> propertySet;
    ComPtr<IMap<HSTRING, IInspectable*>> propMap;
    boolean replaced = false;

    CheckHR(ActivateInstance(
                HStringReference(
                    RuntimeClass_Windows_Foundation_Collections_PropertySet)
                    .Get(),
                &propertySet),
            "Failed to create a PropertySet");
    CheckHR(propertySet.As(&propMap),
            "Failed to convert PropertySet to an IMap");
    CheckHR(propMap->Insert(HStringReference(EGLNativeWindowTypeProperty).Get(),
                            static_cast<IInspectable*>(window), &replaced),
            "Failed to insert to PropertySet");

    // One way to configure the SwapChainPanel is to specify precisely which
    // resolution it should render at. Size customRenderSurfaceSize = Size(800,
    // 600); surfaceCreationProperties->Insert(ref new
    // String(EGLRenderSurfaceSizeProperty),
    // PropertyValue::CreateSize(customRenderSurfaceSize));
    //
    // Another way is to tell the SwapChainPanel to render at a certain scale
    // factor compared to its size. e.g. if the SwapChainPanel is 1920x1280 then
    // setting a factor of 0.5f will make the app render at 960x640 float
    // customResolutionScale = 0.5f; surfaceCreationProperties->Insert(ref new
    // String(EGLRenderResolutionScaleProperty),
    // PropertyValue::CreateSingle(customResolutionScale));
    mEglSurface = eglCreateWindowSurface(
        mEglDisplay, config, static_cast<IInspectable*>(propMap.Get()),
        surfaceAttributes);

    if (mEglSurface == EGL_NO_SURFACE) {
      throw std::exception("eglCreateWindowSurface failed");
    }

    mEglContext = eglCreateContext(mEglDisplay, config, EGL_NO_CONTEXT,
                                   contextAttributes);
    if (mEglContext == EGL_NO_CONTEXT) {
      throw std::exception("eglCreateContext failed");
    }

    if (eglMakeCurrent(mEglDisplay, mEglSurface, mEglSurface, mEglContext) ==
        EGL_FALSE) {
      throw std::exception("eglMakeCurrent failed");
    }
  }

  void CleanupEGL() {
    if (mEglDisplay != EGL_NO_DISPLAY && mEglSurface != EGL_NO_SURFACE) {
      eglDestroySurface(mEglDisplay, mEglSurface);
      mEglSurface = EGL_NO_SURFACE;
    }

    if (mEglDisplay != EGL_NO_DISPLAY && mEglContext != EGL_NO_CONTEXT) {
      eglDestroyContext(mEglDisplay, mEglContext);
      mEglContext = EGL_NO_CONTEXT;
    }

    if (mEglDisplay != EGL_NO_DISPLAY) {
      eglTerminate(mEglDisplay);
      mEglDisplay = EGL_NO_DISPLAY;
    }
  }

  ICoreWindow* mCoreWindow;
  EventRegistrationToken mActivatedToken;
  EventRegistrationToken mVisibilityChangedToken;
  EventRegistrationToken mClosedToken;

  bool mWindowClosed;
  boolean mWindowVisible;

  EGLDisplay mEglDisplay;
  EGLContext mEglContext;
  EGLSurface mEglSurface;

  std::unique_ptr<SimpleRenderer> mCubeRenderer;
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  CheckHR(RoInitialize(RO_INIT_MULTITHREADED), "Failed to initialize COM");
  ComPtr<IFrameworkViewSource> myApp = Make<MyApp>();

  ComPtr<ICoreApplication> coreApp;
  HStringReference coreAppName{
      RuntimeClass_Windows_ApplicationModel_Core_CoreApplication};
  CheckHR(
      RoGetActivationFactory(coreAppName.Get(), __uuidof(ICoreApplication),
                             reinterpret_cast<void**>(coreApp.GetAddressOf())),
      "Failed to get CoreApplication factory");

  coreApp->Run(myApp.Get());

  return 0;
}
