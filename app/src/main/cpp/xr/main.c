#include "engine.h"
#include "input.h"
#include "math.h"
#include "renderer.h"
#include "stdio.h"
#include "stdlib.h"

#include <string.h>

struct XrEngine xr_module_engine;
struct XrInput xr_module_input;
struct XrRenderer xr_module_renderer;
bool xr_initialized = false;
bool xr_usePassthrough = false;

#if defined(_DEBUG)
#include <GLES2/gl2.h>
void GLCheckErrors(const char* file, int line) {
	for (int i = 0; i < 10; i++) {
		const GLenum error = glGetError();
		if (error == GL_NO_ERROR) {
			break;
		}
		ALOGE("OpenGL error on line %s:%d %d", file, line, error);
	}
}

void OXRCheckErrors(XrResult result, const char* file, int line) {
	if (XR_FAILED(result)) {
		char errorBuffer[XR_MAX_RESULT_STRING_SIZE];
		xrResultToString(xr_module_engine.Instance, result, errorBuffer);
        ALOGE("OpenXR error on line %s:%d %s", file, line, errorBuffer);
	}
}
#endif

char gManufacturer[128] = {0};

JNIEXPORT void JNICALL Java_com_winlator_XrActivity_sendManufacturer(JNIEnv *env, jobject thiz, jstring manufacturer) {
    const char *nativeStr = (*env)->GetStringUTFChars(env, manufacturer, 0);
    strncpy(gManufacturer, nativeStr, sizeof(gManufacturer) - 1);
    gManufacturer[sizeof(gManufacturer) - 1] = '\0';
    (*env)->ReleaseStringUTFChars(env, manufacturer, nativeStr);
}

JNIEXPORT void JNICALL Java_com_winlator_XrActivity_init(JNIEnv *env, jobject obj) {

    // Do not allow second initialization
    if (xr_initialized) {
        return;
    }
    if (strcmp(gManufacturer, "PICO") == 0) {
        memset(&xr_module_engine, 0, sizeof(xr_module_engine));
        xr_module_engine.PlatformFlag[PLATFORM_CONTROLLER_PICO] = true;
        xr_module_engine.PlatformFlag[PLATFORM_EXTENSION_INSTANCE] = true;
        xr_module_engine.PlatformFlag[PLATFORM_EXTENSION_PASSTHROUGH] = true;
        xr_module_engine.PlatformFlag[PLATFORM_EXTENSION_PERFORMANCE] = true;
    }
    if (strcmp(gManufacturer, "META") == 0 || strcmp(gManufacturer, "OCULUS") == 0) {
        memset(&xr_module_engine, 0, sizeof(xr_module_engine));
        xr_module_engine.PlatformFlag[PLATFORM_CONTROLLER_QUEST] = true;
        xr_module_engine.PlatformFlag[PLATFORM_EXTENSION_PASSTHROUGH] = true;
        xr_module_engine.PlatformFlag[PLATFORM_EXTENSION_PERFORMANCE] = true;
    }

    // Get Java VM
    JavaVM* vm;
    (*env)->GetJavaVM(env, &vm);

    // Init XR
    xrJava java;
    java.vm = vm;
    java.activity = (*env)->NewGlobalRef(env, obj);
    XrEngineInit(&xr_module_engine, &java, "Winlator", 1);

    // Enter XR
    XrEngineEnter(&xr_module_engine);
    XrInputInit(&xr_module_engine, &xr_module_input);
    XrRendererInit(&xr_module_engine, &xr_module_renderer);
    xr_initialized = true;
    ALOGV("Init called");
}

JNIEXPORT void JNICALL Java_com_winlator_XrActivity_bindFramebuffer(JNIEnv *env, jobject obj) {
    if (xr_initialized) {
        XrRendererBindFramebuffer(&xr_module_renderer);
    }
}

JNIEXPORT jint JNICALL Java_com_winlator_XrActivity_getWidth(JNIEnv *env, jobject obj) {
    int w, h;
    XrRendererGetResolution(&xr_module_engine, &xr_module_renderer, &w, &h);
    return w;
}
JNIEXPORT jint JNICALL Java_com_winlator_XrActivity_getHeight(JNIEnv *env, jobject obj) {
    int w, h;
    XrRendererGetResolution(&xr_module_engine, &xr_module_renderer, &w, &h);
    return h;
}

JNIEXPORT jboolean JNICALL Java_com_winlator_XrActivity_beginFrame(JNIEnv *env, jobject obj, jboolean immersive, jboolean sbs) {
    if (XrRendererInitFrame(&xr_module_engine, &xr_module_renderer)) {

        // Set render canvas
        int mode = immersive ? RENDER_MODE_MONO_6DOF : RENDER_MODE_MONO_SCREEN;
        xr_module_renderer.ConfigFloat[CONFIG_CANVAS_DISTANCE] = 5.0f;
        xr_module_renderer.ConfigInt[CONFIG_PASSTHROUGH] = !immersive && xr_usePassthrough;
        xr_module_renderer.ConfigInt[CONFIG_MODE] = mode;
        xr_module_renderer.ConfigInt[CONFIG_SBS] = sbs;

        // Recenter if mode switched
        static int last_immersive = -1;
        if (last_immersive != immersive) {
            XrRendererRecenter(&xr_module_engine, &xr_module_renderer);
            last_immersive = immersive;
        }

        // Update controllers state
        XrInputUpdate(&xr_module_engine, &xr_module_input);

        // Lock framebuffer
        XrRendererBeginFrame(&xr_module_renderer, 0);

        return true;
    }
    return false;
}

JNIEXPORT void JNICALL Java_com_winlator_XrActivity_endFrame(JNIEnv *env, jobject obj) {
    XrRendererEndFrame(&xr_module_renderer);
    XrRendererFinishFrame(&xr_module_engine, &xr_module_renderer);
}

JNIEXPORT jfloatArray JNICALL Java_com_winlator_XrActivity_getAxes(JNIEnv *env, jobject obj) {
    XrPosef lPose = XrInputGetPose(&xr_module_input, 0);
    XrPosef rPose = XrInputGetPose(&xr_module_input, 1);
    XrVector2f lThumbstick = XrInputGetJoystickState(&xr_module_input, 0);
    XrVector2f rThumbstick = XrInputGetJoystickState(&xr_module_input, 1);
    XrVector3f lPosition = xr_module_renderer.Projections[0].pose.position;
    XrVector3f rPosition = xr_module_renderer.Projections[1].pose.position;
    XrVector3f angles = xr_module_renderer.HmdOrientation;

    int count = 0;
    float data[32];
    data[count++] = XrQuaternionfEulerAngles(lPose.orientation).x; //L_PITCH
    data[count++] = XrQuaternionfEulerAngles(lPose.orientation).y; //L_YAW
    data[count++] = XrQuaternionfEulerAngles(lPose.orientation).z; //L_ROLL
    data[count++] = lThumbstick.x; //L_THUMBSTICK_X
    data[count++] = lThumbstick.y; //L_THUMBSTICK_Y
    data[count++] = lPose.position.x; //L_X
    data[count++] = lPose.position.y; //L_Y
    data[count++] = lPose.position.z; //L_Z
    data[count++] = XrQuaternionfEulerAngles(rPose.orientation).x; //R_PITCH
    data[count++] = XrQuaternionfEulerAngles(rPose.orientation).y; //R_YAW
    data[count++] = XrQuaternionfEulerAngles(rPose.orientation).z; //R_ROLL
    data[count++] = rThumbstick.x; //R_THUMBSTICK_X
    data[count++] = rThumbstick.y; //R_THUMBSTICK_Y
    data[count++] = rPose.position.x; //R_X
    data[count++] = rPose.position.y; //R_Y
    data[count++] = rPose.position.z; //R_Z
    data[count++] = angles.x; //HMD_PITCH
    data[count++] = angles.y; //HMD_YAW
    data[count++] = angles.z; //HMD_ROLL
    data[count++] = (lPosition.x + rPosition.x) * 0.5f; //HMD_X
    data[count++] = (lPosition.y + rPosition.y) * 0.5f; //HMD_Y
    data[count++] = (lPosition.z + rPosition.z) * 0.5f; //HMD_Z
    data[count++] = XrVector3fDistance(lPosition, rPosition); //HMD_IPD


    jfloat values[count];
    memcpy(values, data, count * sizeof(float));
    jfloatArray output = (*env)->NewFloatArray(env, count);
    (*env)->SetFloatArrayRegion(env, output, (jsize)0, (jsize)count, values);
    return output;
}

JNIEXPORT jbooleanArray JNICALL Java_com_winlator_XrActivity_getButtons(JNIEnv *env, jobject obj) {
    uint32_t l = XrInputGetButtonState(&xr_module_input, 0);
    uint32_t r = XrInputGetButtonState(&xr_module_input, 1);

    int count = 0;
    bool data[32];
    data[count++] = l & (int)Grip; //L_GRIP
    data[count++] = l & (int)Enter; //L_MENU
    data[count++] = l & (int)LThumb; //L_THUMBSTICK_PRESS
    data[count++] = l & (int)Left; //L_THUMBSTICK_LEFT
    data[count++] = l & (int)Right; //L_THUMBSTICK_RIGHT
    data[count++] = l & (int)Up; //L_THUMBSTICK_UP
    data[count++] = l & (int)Down; //L_THUMBSTICK_DOWN
    data[count++] = l & (int)Trigger; //L_TRIGGER
    data[count++] = l & (int)X; //L_X
    data[count++] = l & (int)Y; //L_Y
    data[count++] = r & (int)A; //R_A
    data[count++] = r & (int)B; //R_B
    data[count++] = r & (int)Grip; //R_GRIP
    data[count++] = r & (int)RThumb; //R_THUMBSTICK_PRESS
    data[count++] = r & (int)Left; //R_THUMBSTICK_LEFT
    data[count++] = r & (int)Right; //R_THUMBSTICK_RIGHT
    data[count++] = r & (int)Up; //R_THUMBSTICK_UP
    data[count++] = r & (int)Down; //R_THUMBSTICK_DOWN
    data[count++] = r & (int)Trigger; //R_TRIGGER

    jboolean values[count];
    memcpy(values, data, count * sizeof(jboolean));
    jbooleanArray output = (*env)->NewBooleanArray(env, count);
    (*env)->SetBooleanArrayRegion(env, output, (jsize)0, (jsize)count, values);
    return output;
}

JNIEXPORT void JNICALL
Java_com_winlator_XrActivity_nativeSetUsePT(JNIEnv *env, jobject obj, jboolean enabled) {
    xr_usePassthrough = enabled;
}
