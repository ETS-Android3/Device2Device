#include <mutex>
#include <string>
#include <thread>
#ifndef LOG_TAG
#define LOG_TAG "jniComm"
#endif
#include <utils/logging.h>
#include <runtime/TimeStamp.h>
#include <kaics/KaiSocket.h>
#include "../jni/jniInc.h"
#include "texture/TextureView.h"
#include "callback/JavaFuncCalls.h"

extern JavaVM *g_jniJVM;
extern std::string g_className;
extern std::string Jstring2Cstring(JNIEnv *env, jstring jstr);
extern void SetTextView(JNIEnv *env, jclass thiz, const std::string& viewId, const std::string& text);
extern void SetActivityViewText(JNIEnv *env, int viewId, const char* text);

JNIEXPORT void CPP_FUNC_CALL(initJvmEnv)(JNIEnv *env, jclass, jstring class_name)
{
    int state = env->GetJavaVM(&g_jniJVM);
    g_className =
            // Jstring2Cstring(env, getPackageName(env))
            // + "." +
            Jstring2Cstring(env, class_name);
    LOGI("class_name = %s, state = %d.", g_className.c_str(), state);
}

#include "utils/statics.h"

JNIEXPORT jstring CPP_FUNC_CALL(stringGetJNI)(
        JNIEnv *env,
        jobject /* this */)
{
    std::string hello = "C++ string of JNI!";
    char text[16] = {
            0x1a, 0x13, 0x00, 0x07,
            static_cast<char>(0xcc), static_cast<char>(0xff),
            static_cast<char>(0xe0), static_cast<char>(0x88)
    };
    Statics::printBuffer(text, 32);
    return env->NewStringUTF(hello.c_str());
}

JNIEXPORT jlong CPP_FUNC_CALL(timeSetJNI)(JNIEnv *env, jobject, jbyteArray time, jint len)
{
    auto *byte = (unsigned char *) env->GetByteArrayElements(time, nullptr);
    unsigned char stamp[len * 3];
    for (size_t i = 0; i < len; i++) {
        sprintf(reinterpret_cast<char *>(stamp + i * 3), "%02x ", byte[i]);
    }
    LOGI("time hex = %s", stamp);
    uint64_t val =
            (byte[8] & 0xff)
            | (byte[9] << 8 & 0xff00)
            | (byte[10] << 16 & 0xff0000)
            | (byte[11] << 24 & 0xff000000)
            | ((uint64_t) byte[12] << 32 & 0xff00000000)
            | ((uint64_t) byte[13] << 40 & 0xff0000000000)
            | ((uint64_t) byte[14] << 48 & 0xff000000000000)
            | ((uint64_t) byte[15] << 56 & 0xff00000000000000);
    return val;
}

struct PubSubParam {
    std::string addr;
    int port{};
    std::string topic;
    KaiSocket::RECVCALLBACK hook{};
    JNIEnv env{};
    jclass clz{};
    std::string view;
    int id{};
} g_pubSubParam;

void RecvHook(const KaiSocket::Message& msg)
{
    LOGI("topic '%s' of %s, payload: [%s]-[%s].",
         msg.head.topic,
         KaiSocket::G_KaiRole[msg.head.etag],
         msg.data.stat,
         msg.data.body);
    // SetActivityViewText(&g_pubSubParam.env, g_pubSubParam.id, msg.data.body);
}

JNIEXPORT jint CPP_FUNC_CALL(KaiSubscribe)(JNIEnv *env, jclass clz , jstring addr, jint port, jstring topic, jstring viewId, jint id) {
    jint status = -1;
    std::string address = Jstring2Cstring(env, addr);
    g_pubSubParam.addr = address;
    const std::string msg = Jstring2Cstring(env, topic);
    g_pubSubParam.topic = msg;
    g_pubSubParam.port = port;
    g_pubSubParam.hook = RecvHook;
    g_pubSubParam.env = *env;
    g_pubSubParam.clz = clz;
    const std::string view = Jstring2Cstring(env, viewId);
    g_pubSubParam.view = view;
    g_pubSubParam.id = id;
    std::thread th(
            [&status](const PubSubParam& param) -> void {
                KaiSocket kaiSocket;
                kaiSocket.Initialize(param.addr.c_str(), param.port);
                status = kaiSocket.Subscriber(param.topic, param.hook);
                char content[256];
                memset(content, 0, 256);
                sprintf(content, "message from %s:%d, topic = '%s', hook = %p, status = %d",
                        param.addr.c_str(), param.port, param.topic.c_str(),param.hook, status);
                // SetTextView(&param.env, param.clz, param.view, content);
            }, g_pubSubParam);
    if (th.joinable())
        th.detach();
    return status;
}

JNIEXPORT void CPP_FUNC_CALL(KaiPublish)(JNIEnv *env, jclass , jstring topic, jstring payload)
{
    if (g_pubSubParam.addr.empty() || g_pubSubParam.port == 0) {
        LOGI("g_pubSubParam: addr is null or port == 0.");
        return;
    }
    std::thread th([](const std::string& topic, const std::string& payload) {
        KaiSocket kaiSocket;
        kaiSocket.Initialize(g_pubSubParam.addr.c_str(), g_pubSubParam.port);
        LOGI("KaiPublishing to: [%s:%d].", g_pubSubParam.addr.c_str(), g_pubSubParam.port);
        ssize_t stat = kaiSocket.Publisher(topic, payload);
        LOGI("Published(%zu): payload = [%s][%s].", stat, topic.c_str(), payload.c_str());
    }, Jstring2Cstring(env, topic), Jstring2Cstring(env, payload));
    if (th.joinable())
        th.detach();
}

int callback(const char *c, int i)
{
    LOGE("param1 = %s, param2 = %d.", c, i);
    return i;
}

JNIEXPORT void
CPP_FUNC_CALL(callJavaMethod)(JNIEnv *env, jclass, jstring method, jint action, jstring content,
                              jboolean statics)
{
    JavaFuncCalls::GetInstance().CallBack(Jstring2Cstring(env, method),
                                                  static_cast<int>(action),
                                                 Jstring2Cstring(env, content).c_str(),
                                                  statics);
    JavaFuncCalls::CALLBACK call = callback;
    int val = JavaFuncCalls::GetInstance().Register(const_cast<char *>("aaa"), call);
    LOGI("callback = %p, val = %d.", call, val);
}

JNIEXPORT jboolean JNICALL
CPP_FUNC_VIEW(setFileLocate)(JNIEnv *env, jclass, jstring filename)
{
    std::string file_in = Jstring2Cstring(env, filename);
    FILE *fp_in = fopen(file_in.c_str(), "rbe");
    if (nullptr == fp_in) {
        LOGE("open input h264 video file failed, filename [%s]", file_in.c_str());
        return (jboolean) JNI_FALSE;
    }
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
CPP_FUNC_VIEW(updateEglSurface)(JNIEnv *env, jclass, jobject texture, jstring url)
{
    using namespace TextureView;
    int jvs = loadSurfaceView(env, texture);
    if (jvs > 0) {
        LOGI("loaded Surface class: %x", jvs);
    }
    const char *filename = env->GetStringUTFChars(url, JNI_FALSE);
    ANativeWindow *window = initOpenGL(filename);
    if (window != nullptr) {
        LOGD("OpenGL rendering initialized");
        drawRGBColor(1280, 720);
    } else {
        LOGE("native window = null while initOpenGL.");
    }
    env->ReleaseStringUTFChars(url, filename);
}

JNIEXPORT void JNICALL
CPP_FUNC_VIEW(updateSurfaceView)(JNIEnv *env, jclass, jobject texture, jint item)
{
    using namespace TextureView;
    if (item != 1 && item != 2) {
        int jvs = loadSurfaceView(env, texture);
        if (jvs > 0) {
            LOGI("loaded Surface class: %x", jvs);
        }
    }
    switch (item) {
        case 0:
            // No implementation selected
            LOGD("De-initialized");
            return;
        case 1: {
            LOGD("CPU rendering initialized");
            static int iteration = 0;
            static constexpr uint32_t colors[] = {
                    0x00000000,
                    0x0055aaff,
                    0x5500aaff,
                    0xaaff0055,
                    0xff55aa00,
                    0xaa0055ff,
                    0xffffffff
            };
            drawRGBColor(colors[iteration++ % (sizeof(colors) / sizeof(*colors))]);
            break;
        }
        default:
            LOGE("Rendering initialize fail");
            return;
    }
}

JNIEXPORT jlong JNICALL CPP_FUNC_TIME(getAbsoluteTimestamp)(JNIEnv *, jclass)
{
    return TimeStamp::get()->AbsoluteTime();
}

JNIEXPORT jlong JNICALL CPP_FUNC_TIME(getBootTimestamp)(JNIEnv *, jclass)
{
    return TimeStamp::get()->BootTime();
}

#include <unistd.h>
#include <iostream>
#include <files/Pcm2Wav.h>
#include <network/UdpSocket.h>

// #include <template/Clazz1.h>
// #include <template/Clazz2.h>
static int g_msgLen;

JNIEXPORT jint JNICALL
CPP_FUNC_FILE(convertAudioFiles)(JNIEnv *env, jclass, jstring from, jstring save)
{
    return convertAudioFiles(Jstring2Cstring(env, from).c_str(),
                             Jstring2Cstring(env, save).c_str());
}

JNIEXPORT jint JNICALL CPP_FUNC_NETWORK(sendUdpData)(JNIEnv *env, jclass,
                                                     jstring text, jint len)
{
    std::string txt = Jstring2Cstring(env, text);
    const char *tx = txt.c_str();
    LOGI("text = [%s](%d)", tx, len);
    g_msgLen = len;
    auto *sock = new UdpSocket("127.0.0.1", 8899);
    sock->Sender(tx, (unsigned int) len + 1);
    delete sock;
/*
    auto *clz1 = new Clazz1();
    clz1->setBase<Clazz1>("AAA", 3);
    auto *clz2 = new Clazz2();
    clz2->setBase<Clazz2>("22", 2);
*/
    return 0;
}

JNIEXPORT jint JNICALL CPP_FUNC_NETWORK(startServer)(JNIEnv *, jclass)
{
    std::thread th(
            []() -> void {
                int total = g_msgLen + (int)sizeof(NetProtocol);
                char msg[total];
                auto *sock = new UdpSocket();
                int size;
                do {
                    size = sock->Receiver(msg, total);
                    usleep(10000);
                } while (size != 0);
                delete sock;
            }
    );
    if (th.joinable())
        th.detach();
    return 0;
}