//
// Created by H246601 on 2/20/2021.
//
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include <jni.h>
#include <sys/system_properties.h>

#include "includes/elf_util.h"
#include "includes/log.h"

using namespace HookME;

#define pointer_size sizeof(void*)
#define roundUpToPtrSize(v) (v + pointer_size - 1 - ((v + pointer_size - 1) & (pointer_size - 1)))

static u_char* trampolineCode;
static uint32_t trampolineSize;

static uint32_t method_size;
static uint32_t sdk_version;

static uint32_t kAccPublic = 0x0001;
static uint32_t kAccPrivate = 0x0002;
static uint32_t kAccProtected = 0x0004;
static uint32_t kAccStatic = 0x0008;
static uint32_t kAccFinal = 0x0010;
static uint32_t kAccNative = 0x0100;
static uint32_t kAccConstructor = 0x00010000;
static uint32_t kAccFastNative = 0x00080000;
static uint32_t kAccCriticalNative = 0x00200000;
static uint32_t kAccCompileDontBother = 0x01000000;
static uint32_t kAccFastInterpreterToInterpreterInvoke = 0x40000000;
static uint32_t kAccSingleImplementation =  0x08000000;  // method (runtime)
static uint32_t kAccPublicApi = 0x10000000;

static uint32_t OFFSET_entry_point;
static uint32_t OFFSET_access_flags;

jobject (*new_local_ref_)(JNIEnv*,void*) = nullptr;

jclass class_HookRecord;
jmethodID constructor_HookRecord;
jmethodID method_FindRecord;
jfieldID field_artMethod;

static void dump_men(char* point, int size) {
    int c = size / 4;
    if (size % 4 != 0) {
        c++;
    }
    char* buffer = reinterpret_cast<char*>(malloc((c * 11 + 1) * sizeof(char)));
    int j = 0;
    for (int i = 0; i < c; i++) {
        long data = 0;
        memcpy(&data, point + i * 4, 4);
        j += sprintf(buffer + j, i % 2 == 0 ? "0x%08lx " : "0x%08lx\n", data);
    }
    LOGI("Dump mem \npoint at %p, size=%d\n%s", point, size, buffer);
    free(buffer);
}

inline int32_t get_sdk_version() {
    char prop_value[PROP_VALUE_MAX];
    __system_property_get("ro.build.version.sdk", prop_value);
    return atoi(prop_value);
}

inline uint32_t read32(void* addr) {
    return *((uint32_t*) addr);
}

inline void write32(void* addr, uint32_t value) {
    *((uint32_t*) addr) = value;
}

inline uint32_t getAccessFlags(jmethodID method) {
    return read32(reinterpret_cast<char*>(method) + OFFSET_access_flags);
}

inline void setAccessFlags(jmethodID method, uint32_t access_flags) {
    write32(reinterpret_cast<char*>(method) + OFFSET_access_flags, access_flags);
}

inline uint32_t getDeclaringClass(jmethodID method) {
    return read32(reinterpret_cast<char*>(method));
}

inline void setDeclaringClass(jmethodID method, uint32_t declaring_class) {
    write32(reinterpret_cast<char*>(method), declaring_class);
}

jmethodID getRflectedMethod(JNIEnv* env, jobject method) {
    if (sdk_version >= __ANDROID_API_R__) {
        return reinterpret_cast<jmethodID>(env->GetLongField(method, field_artMethod));
    }
    return env->FromReflectedMethod(method);
}

template <typename T>
int findOffset(void* start, T value, size_t size) {
    for (uint offset = 0; offset < size; offset++) {
        T current = *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(start) + offset);
        if (current == value) return static_cast<int>(offset);
    }
    return -1;
}

inline jmethodID findMethodID(JNIEnv* env, jclass clazz, const char *name, const char *sig) {
    jmethodID method = env->GetMethodID(clazz, name, sig);
    return sdk_version < __ANDROID_API_R__ ? method : getRflectedMethod(env,env->ToReflectedMethod(clazz, method, false));
}

static inline size_t getMethodSize(intptr_t a, intptr_t b) {
    intptr_t size = b - a;
    if (size < 0) size = -size;
    return static_cast<size_t>(size);
}

int initOffsetInArtMethod(JNIEnv* env, jclass clazz, ElfImg art_lib) {
#define CLASS_CALLBACK "me/hook/core/HookME$Callback"
#define METHOD_NAME_M1 "beforeCall"
#define METHOD_SIGN_M1 "(Ljava/lang/Object;[Ljava/lang/Object;)Z"
#define METHOD_NAME_M2 "afterCall"
#define METHOD_SIGN_M2 "(Ljava/lang/Object;[Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Throwable;)Ljava/lang/Object;"
    jclass cb_class = env->FindClass(CLASS_CALLBACK);
    jmethodID before_method = findMethodID(env, cb_class, METHOD_NAME_M1, METHOD_SIGN_M1);
    jmethodID after_method = findMethodID(env, cb_class, METHOD_NAME_M2, METHOD_SIGN_M2);
    method_size = getMethodSize(reinterpret_cast<intptr_t>(after_method), reinterpret_cast<intptr_t>(before_method));

    jobjectArray methods = env->NewObjectArray(1, env->FindClass("java/lang/reflect/Method"), nullptr);
    env->SetObjectArrayElement(methods, 0, env->ToReflectedMethod(cb_class, before_method, false));
    jclass class_class = env->FindClass("java/lang/Class");
    jmethodID getClassLoader = env->GetMethodID(class_class, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jclass proxy_class = env->FindClass("java/lang/reflect/Proxy");
    jmethodID generate_proxy = env->GetStaticMethodID(proxy_class, "generateProxy",
            "(Ljava/lang/String;[Ljava/lang/Class;Ljava/lang/ClassLoader;[Ljava/lang/reflect/Method;[[Ljava/lang/Class;)Ljava/lang/Class;");
    jclass cb_proxy_class = static_cast<jclass>(env->CallStaticObjectMethod(
            proxy_class, generate_proxy, env->NewStringUTF(CLASS_CALLBACK"Proxy"), nullptr,
            env->CallObjectMethod(clazz, getClassLoader),
            methods, nullptr));
    jmethodID proxy_beforeCall = findMethodID(env, cb_proxy_class, METHOD_NAME_M1, METHOD_SIGN_M1);
#undef CLASS_CALLBACK
#undef METHOD_NAME_M1
#undef METHOD_SIGN_M1
#undef METHOD_NAME_M2
#undef METHOD_SIGN_M2

    uint32_t expected_access_flags = kAccPublic | kAccFinal | kAccCompileDontBother | kAccSingleImplementation;
    if (sdk_version >= __ANDROID_API_Q__) expected_access_flags |= kAccPublicApi;

    void* art_quick_proxy_invoke_handler = reinterpret_cast<void*>(
            art_lib.getSymbAddress("art_quick_proxy_invoke_handler"));
    OFFSET_access_flags = findOffset(proxy_beforeCall, expected_access_flags, method_size);
    OFFSET_entry_point = findOffset(proxy_beforeCall, art_quick_proxy_invoke_handler, method_size);

    LOGI("method_size=%d OFFSET_access_flags=%d OFFSET_entry_point=%d", method_size, OFFSET_access_flags, OFFSET_entry_point);
    if (OFFSET_access_flags == -1 || OFFSET_entry_point == -1) {
        LOGE("error to find OFFSET_access_flags or OFFSET_entry_point");
        return JNI_ERR;
    }
    return JNI_OK;
}

int initTrampoline(JNIEnv* env) {
#if defined(__aarch64__)
#define SIGN_ARGS "JJ"
#define BRIDGE_OFFSET 20
    u_char trampoline[] = {
            0xe1, 0x03, 0x00, 0xaa,// mov x1, x0    ; set 1st java param
            0xe2, 0x03, 0x00, 0x91,// mov x2, sp    ; set 2nd java param
            0x60, 0x00, 0x00, 0x58,// ldr x0, 12
            0x10, 0x00, 0x40, 0xf8,// ldr x16, [x0, #0x00]
            0x00, 0x02, 0x1f, 0xd6,// br x16
            0x00, 0x00, 0x00, 0x00,// bridge method address
            0x00, 0x00, 0x00, 0x00
    };
    trampoline[13] |= OFFSET_entry_point << 4;
    trampoline[14] |= OFFSET_entry_point >> 4;
#elif defined(__arm__)
#define SIGN_ARGS "II"
#define BRIDGE_OFFSET 16
    u_char trampoline[] = {
            0x00, 0x10, 0xa0, 0xe1,// mov r1, r0    ; set 1st java param
            0x0d, 0x20, 0xa0, 0xe1,// mov r2, sp    ; set 2nd java param
            0x00, 0x00, 0x9f, 0xe5,// ldr r0, [pc, #0]
            0x20, 0xf0, 0x90, 0xe5,// ldr pc, [r0, 0x20]
            0x00, 0x00, 0x00, 0x00 // bridge method address
    };
    trampoline[12] = OFFSET_entry_point;
#elif defined(__x86_64__)
#define SIGN_ARGS "JJ"
#define BRIDGE_OFFSET 8
    u_char trampoline[] = {
            0x48, 0x89, 0xfe,       // movq %rdi, %rsi  ; set 1st java param
            0x48, 0x89, 0xe2,       // movq %rsp, %rdx  ; set 2st java param
            0x48, 0xbf,             // movabs %rdi, 0x0000000000000000 bridge_method Addr
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0xff, 0x77, 0x20,       // push QWORD PTR [rdi + 0x20]
            0xc3                    // ret
    };
    trampoline[18] = OFFSET_entry_point;
#elif defined(__i386__)
#define SIGN_ARGS "II"
#define BRIDGE_OFFSET 5
    u_char trampoline[] = {
        0x89, 0xc1,                 // mov %eax, %ecx   ; set 1st java param
        0x89, 0xe2,                 // mov %esp, %edx   ; set 2st java param
        0xb8,                       // mov eax, 0x00000000 (bridge_method Addr)
        0x00, 0x00, 0x00, 0x00,
        0xff, 0x70, 0x20,           // push DWORD PTR [eax + 0x20]
        0xc3                        // ret
    };
    trampoline[11] = OFFSET_entry_point;
#endif

#define GET_SIGN(X) "(" SIGN_ARGS ")" #X
#define BRIDGE_CLASS "me/hook/core/Bridge" SIGN_ARGS
    trampolineSize = sizeof(trampoline);
    LOGE("trampoline size=%d fixed size=%u", trampolineSize, static_cast<uint32_t>(roundUpToPtrSize(trampolineSize)));

    int buffer_size = roundUpToPtrSize(trampolineSize) * 10;
    trampolineCode = reinterpret_cast<u_char*>(mmap(NULL, buffer_size,
            PROT_READ | PROT_WRITE | PROT_EXEC,
            MAP_ANONYMOUS | MAP_PRIVATE, -1, 0));
    if (trampolineCode == MAP_FAILED) return JNI_ERR;

    const char* methods[10][2] = {
            {"voidBridge", GET_SIGN(V)}, {"objectBridge", GET_SIGN(Ljava/lang/Object;)},
            {"boolBridge", GET_SIGN(Z)}, {"byteBridge", GET_SIGN(B)},
            {"charBridge", GET_SIGN(C)}, {"shortBridge", GET_SIGN(S)},
            {"intBridge", GET_SIGN(I)}, {"floatBridge", GET_SIGN(F)},
            {"longBridge", GET_SIGN(J)}, {"doubleBridge", GET_SIGN(D)},
    };

    jclass clazz = env->FindClass(BRIDGE_CLASS);
    for (int i = 0; i < 10; ++i) {
        unsigned char* targetAddr = reinterpret_cast<u_char*>(trampolineCode) + roundUpToPtrSize(trampolineSize) * i;
        void* proxyMethod= env->GetStaticMethodID(clazz, methods[i][0], methods[i][1]);
        memcpy(targetAddr, trampoline, trampolineSize);
        memcpy(targetAddr + BRIDGE_OFFSET, &proxyMethod, pointer_size);
    }

#undef ABI
#undef BRIDGE_OFFSET
#undef BRIDGE_CLASS
#undef GET_SIGN
#undef SIGN_ARGS
    return JNI_OK;
}

jobject doBackup(JNIEnv* env, jclass targetClazz, jmethodID targetMethod) {
    jmethodID art_backup = reinterpret_cast<jmethodID>(malloc(method_size));
    memcpy(art_backup, targetMethod, method_size);
    uint32_t access_flags = getAccessFlags(art_backup);
    uint32_t old_flags = access_flags;
    if (sdk_version >= __ANDROID_API_N__) {
        access_flags |= kAccCompileDontBother;
    }
    jboolean isStatic = true;
    if ((access_flags & kAccStatic) == 0) {
        access_flags &= ~(kAccPublic | kAccProtected);
        access_flags |= kAccPrivate;
        isStatic = false;
    }
    access_flags &= ~kAccConstructor;
    setAccessFlags(art_backup, access_flags);

    LOGI("change backup access flags from 0x%x to 0x%x", old_flags, access_flags);

    return env->ToReflectedMethod(targetClazz, art_backup, isStatic);
}

void installTrampoline(jmethodID target, char type) {
#define INDEX_FIX '0'
    u_int32_t trampolineIndex = type - INDEX_FIX;
    u_char* trampoline = trampolineCode + roundUpToPtrSize(trampolineSize) * trampolineIndex;
    LOGI("target_method=%p, trampoline=%p, trampolineIndex=%d", target, trampoline, trampolineIndex);
    memcpy(reinterpret_cast<char*>(target) + OFFSET_entry_point, &trampoline, pointer_size);

    if (sdk_version >= __ANDROID_API_N__) {
        int access_flags = getAccessFlags(target);
        int old_flags = access_flags;
        access_flags |= kAccCompileDontBother;
        if (sdk_version >= __ANDROID_API_O__) {
            access_flags |= kAccNative;
        }
        if (sdk_version >= __ANDROID_API_Q__) {
            access_flags &= ~kAccFastInterpreterToInterpreterInvoke;
        }

        if ((access_flags & kAccNative) == 0) {
            access_flags &= ~kAccFastNative;
            if (sdk_version >= __ANDROID_API_P__) {
                access_flags &= ~kAccCriticalNative;
            }
        }
        setAccessFlags(target, access_flags);
        LOGI("change target access flags from 0x%x to 0x%x", old_flags, access_flags);
    }
#undef INDEX_FIX
}

void hook_me_init(JNIEnv* env, jclass clazz) {
    sdk_version = get_sdk_version();
    if (sdk_version >= __ANDROID_API_M__) {
        if (sdk_version >= __ANDROID_API_N_MR1__) {
            kAccCompileDontBother = 0x02000000;
        }
        if (sdk_version >= __ANDROID_API_R__) {
            LOGE("SDK version too high may not support");
        }
    } else {
        LOGE("SDK version %d not supported", sdk_version);
        return;
    }

    ElfImg art_lib("libart.so");
    new_local_ref_ = reinterpret_cast<jobject (*)(JNIEnv*, void*)>(
            art_lib.getSymbAddress("_ZN3art9JNIEnvExt11NewLocalRefEPNS_6mirror6ObjectE"));
    if (new_local_ref_ == nullptr) {
        LOGE("init fail, can't get new_local_ref_");
        return;
    }

    field_artMethod = env->GetFieldID(env->FindClass("java/lang/reflect/Executable"), "artMethod", "J");
    method_FindRecord = env->GetStaticMethodID(clazz, "findRecord", "(J)Lme/hook/core/HookME$HookRecord;");
    class_HookRecord = (jclass)env->NewGlobalRef(env->FindClass("me/hook/core/HookME$HookRecord"));
    constructor_HookRecord = env->GetMethodID(class_HookRecord, "<init>", "(JLjava/lang/reflect/Method;Ljava/lang/String;)V");

    if (initOffsetInArtMethod(env, clazz, art_lib) == JNI_ERR) {
        return;
    }

    if (initTrampoline(env) == JNI_ERR) {
        LOGE("init Trampoline failed");
        return;
    }

}

jobject hook_me_findAndHook(JNIEnv* env, jclass clazz, jclass targetClass, jobject targetMethod, jstring methodShorty) {
    jmethodID target_method = getRflectedMethod(env, targetMethod);
    LOGE("target_method=%p, method_size=%d", target_method, (int)method_size);
    jobject result = env->CallStaticObjectMethod(clazz, method_FindRecord, (jlong)target_method);
    if (result != nullptr) return result;

    const char* method_shorty = env->GetStringUTFChars(methodShorty, NULL);
    jobject backup = doBackup(env, targetClass, target_method);
    installTrampoline(target_method, method_shorty[0]);

    result = env->NewObject(class_HookRecord, constructor_HookRecord,
                            (jlong)target_method, backup, methodShorty);
    env->ReleaseStringUTFChars(methodShorty, method_shorty);
    return result;
}

jcharArray hook_me_getArgArray(JNIEnv* env, jclass, jlong sp, jint argSize) {
#if defined(__aarch64__)
#define ARGS_OFFSET 4
#elif defined(__arm__)
#define ARGS_OFFSET 2
#elif defined(__x86_64__)
#define ARGS_OFFSET 8
#elif defined(__i386__)
#define ARGS_OFFSET 4
#endif
    jchar* args = (jchar*)sp;
    args += ARGS_OFFSET;
    jcharArray arr = env->NewCharArray(argSize/2);
    env->SetCharArrayRegion(arr, 0, argSize/2, args);
#undef ARGS_OFFSET
    return arr;
}

jobject hook_me_getObject(JNIEnv* env, jclass, jlong sp) {
    return new_local_ref_(env, reinterpret_cast<void*>(sp));
}

jboolean hook_me_updateDeclaringClass(JNIEnv* env, jclass, jlong origin, jobject backup) {
    jmethodID artOrigin = reinterpret_cast<jmethodID>(origin);
    jmethodID artBackup = env->FromReflectedMethod(backup);
    uint32_t declaringClass = getDeclaringClass(artOrigin);
    if (declaringClass != getDeclaringClass(artBackup)) {
        LOGI("The declaring_class of method has moved by gc, update its reference in backup method.");
        setDeclaringClass(artBackup, declaringClass);
        return true;
    }
    return false;
}

bool register_Native(JNIEnv* env, jclass clazz) {
    const JNINativeMethod gMethods[] = {
            {"nativeInit", "()V", reinterpret_cast<void*>(hook_me_init)},
            {"findAndHook", "(Ljava/lang/Class;Ljava/lang/reflect/Member;Ljava/lang/String;)Lme/hook/core/HookME$HookRecord;", reinterpret_cast<void*>(hook_me_findAndHook)},
            {"getArgArray", "(JI)[C", reinterpret_cast<void*>(hook_me_getArgArray)},
            {"getObject", "(J)Ljava/lang/Object;", reinterpret_cast<void*>(hook_me_getObject)},
            {"updateDeclaringClass", "(JLjava/lang/reflect/Method;)Z", reinterpret_cast<void*>(hook_me_updateDeclaringClass)},
    };
    return env->RegisterNatives(clazz, gMethods, sizeof(gMethods)/sizeof(gMethods[0])) == JNI_OK;
}

#if defined(DUMP_ASM_CODE)
extern "C" void hook_trampoline(int x0, int x1, int x2, int x3);
#endif

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
#if defined(DUMP_ASM_CODE)
    dump_men(reinterpret_cast<char*>(hook_trampoline), 16);
#endif
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }

    jclass hook_me = env->FindClass("me/hook/core/HookME");
    if (hook_me == nullptr) {
        return JNI_ERR;
    }

    if (!register_Native(env, hook_me)) {
        return JNI_ERR;
    }
    return JNI_VERSION_1_6;
}
