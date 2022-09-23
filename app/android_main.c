#include "CNFGAndroid.h"


#include <android_native_app_glue.h>
#include <jni.h>
#include <native_activity.h>
#include "CNFG.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

struct android_app * gapp;
static int OGLESStarted;
int android_width, android_height;
int android_sdk_version;

	typedef enum
	{
		FBDEV_PIXMAP_DEFAULT = 0,
		FBDEV_PIXMAP_SUPPORTS_UMP = (1<<0),
		FBDEV_PIXMAP_ALPHA_FORMAT_PRE = (1<<1),
		FBDEV_PIXMAP_COLORSPACE_sRGB = (1<<2),
		FBDEV_PIXMAP_EGL_MEMORY = (1<<3)        /* EGL allocates/frees this memory */
	} fbdev_pixmap_flags;

	typedef struct fbdev_window
	{
		unsigned short width;
		unsigned short height;
	} fbdev_window;

	typedef struct fbdev_pixmap
	{
		unsigned int height;
		unsigned int width;
		unsigned int bytes_per_pixel;
		unsigned char buffer_size;
		unsigned char red_size;
		unsigned char green_size;
		unsigned char blue_size;
		unsigned char alpha_size;
		unsigned char luminance_size;
		fbdev_pixmap_flags flags;
		unsigned short *data;
		unsigned int format;
	} fbdev_pixmap;

EGLNativeWindowType native_window;



static EGLint const config_attribute_list[] = {
	EGL_RED_SIZE, 8,
	EGL_GREEN_SIZE, 8,
	EGL_BLUE_SIZE, 8,
	EGL_ALPHA_SIZE, 8,
	EGL_BUFFER_SIZE, 32,
	EGL_STENCIL_SIZE, 0,
	EGL_DEPTH_SIZE, 16,
	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
	EGL_NONE
};


static EGLint window_attribute_list[] = {
	EGL_NONE
};

static const EGLint context_attribute_list[] = {
	EGL_CONTEXT_CLIENT_VERSION, 2,
	EGL_NONE
};

EGLDisplay egl_display;
EGLSurface egl_surface;


static void AndroidMakeFullscreen()
{
	//Partially based on https://stackoverflow.com/questions/47507714/how-do-i-enable-full-screen-immersive-mode-for-a-native-activity-ndk-app
	const struct JNINativeInterface * env = 0;
	const struct JNINativeInterface ** envptr = &env;
	const struct JNIInvokeInterface ** jniiptr = gapp->activity->vm;
	const struct JNIInvokeInterface * jnii = *jniiptr;

	jnii->AttachCurrentThread( jniiptr, &envptr, NULL);
	env = (*envptr);

	//Get android.app.NativeActivity, then get getWindow method handle, returns view.Window type
	jclass activityClass = env->FindClass( envptr, "android/app/NativeActivity");
	jmethodID getWindow = env->GetMethodID( envptr, activityClass, "getWindow", "()Landroid/view/Window;");
	jobject window = env->CallObjectMethod( envptr, gapp->activity->clazz, getWindow);

	//Get android.view.Window class, then get getDecorView method handle, returns view.View type
	jclass windowClass = env->FindClass( envptr, "android/view/Window");
	jmethodID getDecorView = env->GetMethodID( envptr, windowClass, "getDecorView", "()Landroid/view/View;");
	jobject decorView = env->CallObjectMethod( envptr, window, getDecorView);

	//Get the flag values associated with systemuivisibility
	jclass viewClass = env->FindClass( envptr, "android/view/View");
	const int flagLayoutHideNavigation = env->GetStaticIntField( envptr, viewClass, env->GetStaticFieldID( envptr, viewClass, "SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION", "I"));
	const int flagLayoutFullscreen = env->GetStaticIntField( envptr, viewClass, env->GetStaticFieldID( envptr, viewClass, "SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN", "I"));
	const int flagLowProfile = env->GetStaticIntField( envptr, viewClass, env->GetStaticFieldID( envptr, viewClass, "SYSTEM_UI_FLAG_LOW_PROFILE", "I"));
	const int flagHideNavigation = env->GetStaticIntField( envptr, viewClass, env->GetStaticFieldID( envptr, viewClass, "SYSTEM_UI_FLAG_HIDE_NAVIGATION", "I"));
	const int flagFullscreen = env->GetStaticIntField( envptr, viewClass, env->GetStaticFieldID( envptr, viewClass, "SYSTEM_UI_FLAG_FULLSCREEN", "I"));
	const int flagImmersiveSticky = env->GetStaticIntField( envptr, viewClass, env->GetStaticFieldID( envptr, viewClass, "SYSTEM_UI_FLAG_IMMERSIVE_STICKY", "I"));

	jmethodID setSystemUiVisibility = env->GetMethodID( envptr, viewClass, "setSystemUiVisibility", "(I)V");

	//Call the decorView.setSystemUiVisibility(FLAGS)
	env->CallVoidMethod( envptr, decorView, setSystemUiVisibility,
		        (flagLayoutHideNavigation | flagLayoutFullscreen | flagLowProfile | flagHideNavigation | flagFullscreen | flagImmersiveSticky));

	//now set some more flags associated with layoutmanager -- note the $ in the class path
	//search for api-versions.xml
	//https://android.googlesource.com/platform/development/+/refs/tags/android-9.0.0_r48/sdk/api-versions.xml

	jclass layoutManagerClass = env->FindClass( envptr, "android/view/WindowManager$LayoutParams");
	const int flag_WinMan_Fullscreen = env->GetStaticIntField( envptr, layoutManagerClass, (env->GetStaticFieldID( envptr, layoutManagerClass, "FLAG_FULLSCREEN", "I") ));
	const int flag_WinMan_KeepScreenOn = env->GetStaticIntField( envptr, layoutManagerClass, (env->GetStaticFieldID( envptr, layoutManagerClass, "FLAG_KEEP_SCREEN_ON", "I") ));
	const int flag_WinMan_hw_acc = env->GetStaticIntField( envptr, layoutManagerClass, (env->GetStaticFieldID( envptr, layoutManagerClass, "FLAG_HARDWARE_ACCELERATED", "I") ));
	//    const int flag_WinMan_flag_not_fullscreen = env->GetStaticIntField(layoutManagerClass, (env->GetStaticFieldID(layoutManagerClass, "FLAG_FORCE_NOT_FULLSCREEN", "I") ));
	//call window.addFlags(FLAGS)
	env->CallVoidMethod( envptr, window, (env->GetMethodID (envptr, windowClass, "addFlags" , "(I)V")), (flag_WinMan_Fullscreen | flag_WinMan_KeepScreenOn | flag_WinMan_hw_acc));

	jnii->DetachCurrentThread( jniiptr );
}


static short iLastInternalW, iLastInternalH;

void CNFGSwapBuffers()
{
	CNFGFlushRender();
	eglSwapBuffers(egl_display, egl_surface);
	android_width = ANativeWindow_getWidth( native_window );
	android_height = ANativeWindow_getHeight( native_window );
	glViewport( 0, 0, android_width, android_height );
	if( iLastInternalW != android_width || iLastInternalH != android_height )
		CNFGInternalResize( iLastInternalW=android_width, iLastInternalH=android_height );
}

void CNFGGetDimensions( short * x, short * y )
{
	*x = android_width;
	*y = android_height;
	if( *x != iLastInternalW || *y != iLastInternalH )
		CNFGInternalResize( iLastInternalW=*x, iLastInternalH=*y );
}

int CNFGSetup( const char * WindowName, int w, int h )
{
	EGLint egl_major, egl_minor;
	EGLConfig config;
	EGLint num_config;
	EGLContext context;

	//This MUST be called before doing any initialization.
	int events;
	while( !OGLESStarted )
	{
		struct android_poll_source* source;
		if (ALooper_pollAll( 0, 0, &events, (void**)&source) >= 0)
		{
			if (source != NULL) source->process(gapp, source);
		}
	}


	egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (egl_display == EGL_NO_DISPLAY) {
		printf( "Error: No display found!\n");
		return -1;
	}

	if (!eglInitialize(egl_display, &egl_major, &egl_minor)) {
		printf( "Error: eglInitialise failed!\n");
		return -1;
	}

	eglChooseConfig(egl_display, config_attribute_list, &config, 1,
			&num_config);
	printf( "Config: %d\n", num_config );

	printf( "Creating Context\n" );
	context = eglCreateContext(egl_display, config, EGL_NO_CONTEXT,
//				NULL );
				context_attribute_list);
	if (context == EGL_NO_CONTEXT) {
		printf( "Error: eglCreateContext failed: 0x%08X\n",
			eglGetError());
		return -1;
	}
	printf( "Context Created %p\n", context );


	if( native_window && !gapp->window )
	{
		printf( "WARNING: App restarted without a window.  Cannot progress.\n" );
		exit( 0 );
	}

	printf( "Getting Surface %p\n", native_window = gapp->window );

	if( !native_window )
	{
		printf( "FAULT: Cannot get window\n" );
		return -5;
	}
	android_width = ANativeWindow_getWidth( native_window );
	android_height = ANativeWindow_getHeight( native_window );
	printf( "Width/Height: %dx%d\n", android_width, android_height );
	egl_surface = eglCreateWindowSurface(egl_display, config,
			     gapp->window,
			     window_attribute_list);
	printf( "Got Surface: %p\n", egl_surface );

	if (egl_surface == EGL_NO_SURFACE) {
		printf( "Error: eglCreateWindowSurface failed: "
			"0x%08X\n", eglGetError());
		return -1;
	}

	if (!eglMakeCurrent(egl_display, egl_surface, egl_surface, context)) {
		printf( "Error: eglMakeCurrent() failed: 0x%08X\n",
			eglGetError());
		return -1;
	}


	CNFGSetupBatchInternal();

	{
		short dummyx, dummyy;
		CNFGGetDimensions( &dummyx, &dummyy );
	}

	return 0;
}

void CNFGSetupFullscreen( const char * WindowName, int screen_number )
{
	//Removes decoration, must be called before setup.
	AndroidMakeFullscreen();

	CNFGSetup( WindowName, -1, -1 );
}

int debuga, debugb, debugc;

int32_t handle_input(struct android_app* app, AInputEvent* event)
{
	//Potentially do other things here.

	if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION)
	{
		int action = AMotionEvent_getAction( event );
		int whichsource = action >> 8;
		action &= AMOTION_EVENT_ACTION_MASK;
		size_t pointerCount = AMotionEvent_getPointerCount(event);

		for (size_t i = 0; i < pointerCount; ++i)
		{
			int x, y, index;
			x = AMotionEvent_getX(event, i);
			y = AMotionEvent_getY(event, i);
			index = AMotionEvent_getPointerId( event, i );

			if( action == AMOTION_EVENT_ACTION_POINTER_DOWN || action == AMOTION_EVENT_ACTION_DOWN )
			{
				int id = index;
				if( action == AMOTION_EVENT_ACTION_POINTER_DOWN && id != whichsource ) continue;
				HandleButton( x, y, id, 1 );
				ANativeActivity_showSoftInput( gapp->activity, ANATIVEACTIVITY_SHOW_SOFT_INPUT_FORCED );
			}
			else if( action == AMOTION_EVENT_ACTION_POINTER_UP || action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_CANCEL )
			{
				int id = index;
				if( action == AMOTION_EVENT_ACTION_POINTER_UP && id != whichsource ) continue;
				HandleButton( x, y, id, 0 );
			}
			else if( action == AMOTION_EVENT_ACTION_MOVE )
			{
				HandleMotion( x, y, index );
			}
		}
		return 1;
	}
	else if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_KEY)
	{
		int code = AKeyEvent_getKeyCode(event);
		int unicode = AndroidGetUnicodeChar( code, AMotionEvent_getMetaState( event ) );
		if( unicode )
			HandleKey( unicode, AKeyEvent_getAction(event) );
		else
		{
			HandleKey( code, !AKeyEvent_getAction(event) );
			return (code == 4)?1:0; //don't override functionality.
		}

		return 1;
	}
	return 0;
}

void CNFGHandleInput()
{

	int events;
	struct android_poll_source* source;
	while( ALooper_pollAll( 0, 0, &events, (void**)&source) >= 0 )
	{
		if (source != NULL)
		{
			source->process(gapp, source);
		}
	}

}




void handle_cmd(struct android_app* app, int32_t cmd)
{
	switch (cmd)
	{
	case APP_CMD_DESTROY:
		//This gets called initially after back.
		HandleDestroy();
		ANativeActivity_finish( gapp->activity );
		break;
	case APP_CMD_INIT_WINDOW:
		//When returning from a back button suspension, this isn't called.
		if( !OGLESStarted )
		{
			OGLESStarted = 1;
			printf( "Got start event\n" );
		}
		else
		{
			CNFGSetup( "", -1, -1 );
			HandleResume();
		}
		break;
	//case APP_CMD_TERM_WINDOW:
		//This gets called initially when you click "back"
		//This also gets called when you are brought into standby.
		//Not sure why - callbacks here seem to break stuff.
	//	break;
	default:
		printf( "event not handled: %d\n", cmd);
	}
}

int __system_property_get(const char* name, char* value);


void AndroidDisplayKeyboard(int pShow)
{
	//Based on https://stackoverflow.com/questions/5864790/how-to-show-the-soft-keyboard-on-native-activity
	jint lFlags = 0;
	const struct JNINativeInterface * env = 0;
	const struct JNINativeInterface ** envptr = &env;
	const struct JNIInvokeInterface ** jniiptr = gapp->activity->vm;
	const struct JNIInvokeInterface * jnii = *jniiptr;

	jnii->AttachCurrentThread( jniiptr, &envptr, NULL);
	env = (*envptr);
	jclass activityClass = env->FindClass( envptr, "android/app/NativeActivity");

	// Retrieves NativeActivity.
	jobject lNativeActivity = gapp->activity->clazz;


	// Retrieves Context.INPUT_METHOD_SERVICE.
	jclass ClassContext = env->FindClass( envptr, "android/content/Context");
	jfieldID FieldINPUT_METHOD_SERVICE = env->GetStaticFieldID( envptr, ClassContext, "INPUT_METHOD_SERVICE", "Ljava/lang/String;" );
	jobject INPUT_METHOD_SERVICE = env->GetStaticObjectField( envptr, ClassContext, FieldINPUT_METHOD_SERVICE );

	// Runs getSystemService(Context.INPUT_METHOD_SERVICE).
	jclass ClassInputMethodManager = env->FindClass( envptr, "android/view/inputmethod/InputMethodManager" );
	jmethodID MethodGetSystemService = env->GetMethodID( envptr, activityClass, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
	jobject lInputMethodManager = env->CallObjectMethod( envptr, lNativeActivity, MethodGetSystemService, INPUT_METHOD_SERVICE);

	// Runs getWindow().getDecorView().
	jmethodID MethodGetWindow = env->GetMethodID( envptr, activityClass, "getWindow", "()Landroid/view/Window;");
	jobject lWindow = env->CallObjectMethod( envptr, lNativeActivity, MethodGetWindow);
	jclass ClassWindow = env->FindClass( envptr, "android/view/Window");
	jmethodID MethodGetDecorView = env->GetMethodID( envptr, ClassWindow, "getDecorView", "()Landroid/view/View;");
	jobject lDecorView = env->CallObjectMethod( envptr, lWindow, MethodGetDecorView);

	if (pShow) {
		// Runs lInputMethodManager.showSoftInput(...).
		jmethodID MethodShowSoftInput = env->GetMethodID( envptr, ClassInputMethodManager, "showSoftInput", "(Landroid/view/View;I)Z");
		/*jboolean lResult = */env->CallBooleanMethod( envptr, lInputMethodManager, MethodShowSoftInput, lDecorView, lFlags);
	} else {
		// Runs lWindow.getViewToken()
		jclass ClassView = env->FindClass( envptr, "android/view/View");
		jmethodID MethodGetWindowToken = env->GetMethodID( envptr, ClassView, "getWindowToken", "()Landroid/os/IBinder;");
		jobject lBinder = env->CallObjectMethod( envptr, lDecorView, MethodGetWindowToken);

		// lInputMethodManager.hideSoftInput(...).
		jmethodID MethodHideSoftInput = env->GetMethodID( envptr, ClassInputMethodManager, "hideSoftInputFromWindow", "(Landroid/os/IBinder;I)Z");
		/*jboolean lRes = */env->CallBooleanMethod( envptr, lInputMethodManager, MethodHideSoftInput, lBinder, lFlags);
	}

	// Finished with the JVM.
	jnii->DetachCurrentThread( jniiptr );
}

int AndroidGetUnicodeChar( int keyCode, int metaState )
{
	//https://stackoverflow.com/questions/21124051/receive-complete-android-unicode-input-in-c-c/43871301

	int eventType = AKEY_EVENT_ACTION_DOWN;
	const struct JNINativeInterface * env = 0;
	const struct JNINativeInterface ** envptr = &env;
	const struct JNIInvokeInterface ** jniiptr = gapp->activity->vm;
	const struct JNIInvokeInterface * jnii = *jniiptr;

	jnii->AttachCurrentThread( jniiptr, &envptr, NULL);
	env = (*envptr);
	//jclass activityClass = env->FindClass( envptr, "android/app/NativeActivity");
	// Retrieves NativeActivity.
	//jobject lNativeActivity = gapp->activity->clazz;

	jclass class_key_event = env->FindClass( envptr, "android/view/KeyEvent");
	int unicodeKey;

	jmethodID method_get_unicode_char = env->GetMethodID( envptr, class_key_event, "getUnicodeChar", "(I)I");
	jmethodID eventConstructor = env->GetMethodID( envptr, class_key_event, "<init>", "(II)V");
	jobject eventObj = env->NewObject( envptr, class_key_event, eventConstructor, eventType, keyCode);

	unicodeKey = env->CallIntMethod( envptr, eventObj, method_get_unicode_char, metaState );

	// Finished with the JVM.
	jnii->DetachCurrentThread( jniiptr );

	//printf("Unicode key is: %d", unicodeKey);
	return unicodeKey;
}


//Based on: https://stackoverflow.com/questions/41820039/jstringjni-to-stdstringc-with-utf8-characters

jstring android_permission_name(const struct JNINativeInterface ** envptr, const char* perm_name) {
    // nested class permission in class android.Manifest,
    // hence android 'slash' Manifest 'dollar' permission
	const struct JNINativeInterface * env = *envptr;
    jclass ClassManifestpermission = env->FindClass( envptr, "android/Manifest$permission");
    jfieldID lid_PERM = env->GetStaticFieldID( envptr, ClassManifestpermission, perm_name, "Ljava/lang/String;" );
    jstring ls_PERM = (jstring)(env->GetStaticObjectField( envptr, ClassManifestpermission, lid_PERM )); 
    return ls_PERM;
}

/**
 * \brief Tests whether a permission is granted.
 * \param[in] app a pointer to the android app.
 * \param[in] perm_name the name of the permission, e.g.,
 *   "READ_EXTERNAL_STORAGE", "WRITE_EXTERNAL_STORAGE".
 * \retval true if the permission is granted.
 * \retval false otherwise.
 * \note Requires Android API level 23 (Marshmallow, May 2015)
 */
int AndroidHasPermissions( const char* perm_name)
{
	struct android_app* app = gapp;
	const struct JNINativeInterface * env = 0;
	const struct JNINativeInterface ** envptr = &env;
	const struct JNIInvokeInterface ** jniiptr = app->activity->vm;
	const struct JNIInvokeInterface * jnii = *jniiptr;

	if( android_sdk_version < 23 )
	{
		printf( "Android SDK version %d does not support AndroidHasPermissions\n", android_sdk_version );
		return 1;
	}

	jnii->AttachCurrentThread( jniiptr, &envptr, NULL);
	env = (*envptr);

	int result = 0;
	jstring ls_PERM = android_permission_name( envptr, perm_name);

	jint PERMISSION_GRANTED = (-1);

	{
		jclass ClassPackageManager = env->FindClass( envptr, "android/content/pm/PackageManager" );
		jfieldID lid_PERMISSION_GRANTED = env->GetStaticFieldID( envptr, ClassPackageManager, "PERMISSION_GRANTED", "I" );
		PERMISSION_GRANTED = env->GetStaticIntField( envptr, ClassPackageManager, lid_PERMISSION_GRANTED );
	}
	{
		jobject activity = app->activity->clazz;
		jclass ClassContext = env->FindClass( envptr, "android/content/Context" );
		jmethodID MethodcheckSelfPermission = env->GetMethodID( envptr, ClassContext, "checkSelfPermission", "(Ljava/lang/String;)I" );
		jint int_result = env->CallIntMethod( envptr, activity, MethodcheckSelfPermission, ls_PERM );
		result = (int_result == PERMISSION_GRANTED);
	}

	jnii->DetachCurrentThread( jniiptr );

	return result;
}

/**
 * \brief Query file permissions.
 * \details This opens the system dialog that lets the user
 *  grant (or deny) the permission.
 * \param[in] app a pointer to the android app.
 * \note Requires Android API level 23 (Marshmallow, May 2015)
 */
void AndroidRequestAppPermissions(const char * perm)
{
	if( android_sdk_version < 23 )
	{
		printf( "Android SDK version %d does not support AndroidRequestAppPermissions\n",android_sdk_version );
		return;
	}

	struct android_app* app = gapp;
	const struct JNINativeInterface * env = 0;
	const struct JNINativeInterface ** envptr = &env;
	const struct JNIInvokeInterface ** jniiptr = app->activity->vm;
	const struct JNIInvokeInterface * jnii = *jniiptr;
	jnii->AttachCurrentThread( jniiptr, &envptr, NULL);
	env = (*envptr);
	jobject activity = app->activity->clazz;

	jobjectArray perm_array = env->NewObjectArray( envptr, 1, env->FindClass( envptr, "java/lang/String"), env->NewStringUTF( envptr, "" ) );
	env->SetObjectArrayElement( envptr, perm_array, 0, android_permission_name( envptr, perm ) );
	jclass ClassActivity = env->FindClass( envptr, "android/app/Activity" );

	jmethodID MethodrequestPermissions = env->GetMethodID( envptr, ClassActivity, "requestPermissions", "([Ljava/lang/String;I)V" );

	// Last arg (0) is just for the callback (that I do not use)
	env->CallVoidMethod( envptr, activity, MethodrequestPermissions, perm_array, 0 );
	jnii->DetachCurrentThread( jniiptr );
}

void AndroidSendToBack( int param )
{
	struct android_app* app = gapp;
	const struct JNINativeInterface * env = 0;
	const struct JNINativeInterface ** envptr = &env;
	const struct JNIInvokeInterface ** jniiptr = app->activity->vm;
	const struct JNIInvokeInterface * jnii = *jniiptr;
	jnii->AttachCurrentThread( jniiptr, &envptr, NULL);
	env = (*envptr);
	jobject activity = app->activity->clazz;

	//_glfmCallJavaMethodWithArgs(jni, gapp->activity->clazz, "moveTaskToBack", "(Z)Z", Boolean, false);
	jclass ClassActivity = env->FindClass( envptr, "android/app/Activity" );
	jmethodID MethodmoveTaskToBack = env->GetMethodID( envptr, ClassActivity, "moveTaskToBack", "(Z)Z" );
	env->CallBooleanMethod( envptr, activity, MethodmoveTaskToBack, param );
	jnii->DetachCurrentThread( jniiptr );
}


void android_main(struct android_app* app)
{
    printf("Starting android_main\n");
	int main( int argc, char ** argv );
	char * argv[] = { "main", 0 };

	{
		char sdk_ver_str[92];
		int len = __system_property_get("ro.build.version.sdk", sdk_ver_str);
		if( len <= 0 ) 
			android_sdk_version = 0;
		else
			android_sdk_version = atoi(sdk_ver_str);
	}

	gapp = app;
	app->onAppCmd = handle_cmd;
	app->onInputEvent = handle_input;

	main( 1, argv );
}

