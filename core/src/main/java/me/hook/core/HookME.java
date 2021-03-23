package me.hook.core;

import android.os.Build;
import android.util.Log;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

public final class HookME {

    private final static String TAG = "HookME";

    private final static char INDEX_FIX = '0';
    private final static Map<Long, HookRecord> recordMap = new HashMap<Long, HookRecord>();

    private static boolean debug = true;
    private static boolean GC_CHECK = false;
    static {
        System.loadLibrary(TAG);
    }

    private HookME() {
    }

    private static HookRecord findRecord(long target) {
        return recordMap.get(target);
    }

    private static char getShortyType(Class<?> clazz) {
        char type = 1;
        if (void.class == clazz ) type = 0;
        else if (boolean.class == clazz) type = 2;
        else if (byte.class == clazz) type = 3;
        else if (char.class == clazz) type = 4;
        else if (short.class == clazz) type = 5;
        else if (int.class == clazz) type = 6;
        else if (float.class == clazz) type = 7;
        else if (long.class == clazz) type = 8;
        else if (double.class == clazz) type = 9;
        return (char)(type + INDEX_FIX);
    }

    private static String getShorty(Class<?> returnType, Class<?>[] paramTypes) {
        StringBuilder shorty = new StringBuilder();
        shorty.append(getShortyType(returnType));
        for (Class<?> type: paramTypes) {
            shorty.append(getShortyType(type));
        }
        return shorty.toString();
    }

    private static void resolve(Method method) {
        Object[] badArgs;
        if (method.getParameterTypes().length > 0) {
            badArgs = null;
        } else {
            badArgs = new Object[1];
        }
        try {
            method.invoke(null, badArgs);
        } catch (IllegalArgumentException e) {
            return;
        } catch (Exception e) {
            throw new RuntimeException("Unknown exception thrown when resolve static method.", e);
        }
        throw new RuntimeException("No IllegalArgumentException thrown when resolve static method.");
    }

    public static Callback.UnHook doHook(Member member, Callback callback) {
        if (debug) {
            Log.d(TAG, "start hook " + member);
        }

        Class<?> returnType = null;
        Class<?>[] paramTypes = null;
        if (member instanceof Method) {
            Method method = (Method) member;
            returnType = method.getReturnType();
            paramTypes = method.getParameterTypes();
            if (Modifier.isStatic(method.getModifiers())) {
                resolve(method);
            }
        } else {
            Constructor constructor = (Constructor) member;
            returnType = void.class;
            paramTypes = constructor.getParameterTypes();
        }

        HookRecord record = findAndHook(member.getDeclaringClass(), member,
                getShorty(returnType, paramTypes));
        if (record == null) {
            Log.e(TAG, "hook failed for " + member);
            return null;
        }
        if (debug) {
            Log.i(TAG, String.format("hook success artMethod=%#x, shorty=%s", record.target, record.shorty));
        }
        record.backup.setAccessible(true);
        record.original = member;
        // TODO application will crash when hook proxy method and call record.backup.toGenericString()
        // Log.i(TAG, "backup: "+record.backup.toGenericString());
        recordMap.put(record.target, record);
        record.addCallback(callback);
        return callback.new UnHook(record);
    }

    private static Object handleCall(HookRecord record, Object receiver, Object[] args) throws  Throwable{
        Object result = null;
        Throwable throwable = null;

        List<Callback> cbs = record.callbacks;
        boolean execAll = true;
        int len = cbs.size();
        int callIndex = -1;
        while (execAll && ++callIndex < len) {
            try {
                execAll = !cbs.get(callIndex).beforeCall(receiver, args);
                if (!execAll && debug) {
                    Log.i(TAG, "skip remain callbacks and backup method");
                }
            } catch (Exception e) {
                execAll = false;
                Log.e(TAG, "Unexpected exception occurred when calling " + cbs.get(callIndex).getClass().getName() + ".beforeCall()", e);
                callIndex++;
                break;
            }
        }

        if (execAll) {
            try {
                Method backup = record.backup;
                if (record.update) {
                    Log.i(TAG, "try to call updateDeclaringClass");
                    updateDeclaringClass(record.target, backup);
                    record.update = false;
                }
                result = backup.invoke(receiver, args);
            } catch (InvocationTargetException e) {
                throwable = e.getTargetException();
            }
        }

        while (--callIndex >= 0) {
            try {
                result = cbs.get(callIndex).afterCall(receiver, args, result, throwable);
            } catch (Exception e) {
                Log.e(TAG, "Unexpected exception occurred when calling " + cbs.get(callIndex).getClass().getName() + ".afterCall()", e);
            }
        }

        if (throwable != null) throw throwable;
        return result;
    }

    protected static Object handleBridge(long addr, long sp) throws Throwable {
        if (debug) {
            Log.i(TAG, String.format("handleBridge: artMethod=%#x, sp=%#x", addr, sp));
        }

        HookRecord record = findRecord(addr);
        if (record == null) {
            throw new AssertionError("Not found HookRecord for ArtMethod pointer 0x" + Long.toHexString(addr));
        }

        char shorty[] = record.shorty.toCharArray();
        /*
         * use char array to store args.
         * char is unsigned, easy to use than int
         */
        char []argArr = getArgArray(sp, record.argSize);
        int index = 0;
        Object receiver = null;
        Object []args = new Object[shorty.length - 1];
        if (!record.isStatic) {
            receiver = getObject(argArr[index++] | argArr[index++] << 16);
        }
        for (int i = 0; i < shorty.length - 1; i++) {
            int type = shorty[i + 1] - '0';
            switch (type) {
                case 1:
                    args[i] = getObject(argArr[index++] | argArr[index++] << 16);
                    break;
                case 2:
                    args[i] = argArr[index++] == 1;
                    index++;
                    break;
                case 3:
                    args[i] = (byte)argArr[index++];
                    index++;
                    break;
                case 4:
                    args[i] = argArr[index++];
                    index++;
                    break;
                case 5:
                    args[i] = (short)argArr[index++];
                    index++;
                    break;
                case 6:
                    args[i] = argArr[index++] | argArr[index++] << 16;
                    break;
                case 7:
                    args[i] = Float.intBitsToFloat(argArr[index++] | argArr[index++] << 16);
                    break;
                case 8:
                    args[i] = (long)argArr[index++] | (long)argArr[index++] << 16
                            | (long)argArr[index++] << 32 | (long)argArr[index++] << 48;
                    break;
                case 9:
                    args[i] = Double.longBitsToDouble((long)argArr[index++]
                            | (long)argArr[index++] << 16 | (long)argArr[index++] << 32
                            | (long)argArr[index++] << 48);
                    break;
            }
        }
        if (debug) {
            Log.i(TAG, String.format("%s receiver: %s, args: %s", record.original.toString(), receiver, Arrays.toString(args)));
        }
        return handleCall(record, receiver, args);
    }

    public static void init() {
        nativeInit();
        final boolean needUpdate = Build.VERSION.SDK_INT >= Build.VERSION_CODES.N;
        if (GC_CHECK && needUpdate) {
            try {
                doHook(Runtime.class.getDeclaredMethod("gc"), new Callback() {
                    @Override
                    public boolean beforeCall(Object receiver, Object[] args) throws Exception {
                        Log.i(TAG, "beforeCall gc...");
                        if (!needUpdate) return false;
                        synchronized (recordMap) {
                            for (HookME.HookRecord record : recordMap.values()) {
                                record.update = needUpdate;
                            }
                        }
                        return false;
                    }

                    @Override
                    public Object afterCall(Object receiver, Object[] args, Object result, Throwable throwable) throws Exception {
                        return null;
                    }
                });
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

    public static native void nativeInit();
    private static native HookRecord findAndHook(Class<?> targetClass, Member targetMethod, String methodSign);
    private static native char[] getArgArray(long sp, int size);
    private static native Object getObject(long sp);
    private static native boolean updateDeclaringClass(long origin, Method backup);

    public static final class HookRecord {
        public final long target;
        public final Method backup;
        public final boolean isStatic;
        public final String shorty;
        public final int argSize;
        private Member original;
        private boolean update;
        private List<Callback> callbacks =
                Collections.synchronizedList(new LinkedList<Callback>());

        private HookRecord(long target, Method backup, String shorty) {
            this.target = target;
            this.backup = backup;
            this.isStatic = Modifier.isStatic(backup.getModifiers());
            this.shorty = shorty;
            this.argSize = getArgSize(isStatic, shorty);
            this.update = false;
        }

        private void addCallback(Callback callback) {
            if (callbacks.contains(callback)) {
                Log.w(TAG, "this callback already added!");
                return;
            }
            callbacks.add(callback);
        }

        public void removeCallback(Callback callback) {
            callbacks.remove(callback);
        }

        private static int getArgSize(boolean isStatic, String shorty) {
            int size = isStatic ? 0 : 4;
            int arg_num = shorty.length() - 1;
            for (int i = 0; i < arg_num; i++) {
                int type = shorty.charAt(i + 1) - INDEX_FIX;
                switch (type) {
                    case 8:
                    case 9:
                        size += 4;
                    default:
                        size += 4;
                        break;
                }
            }
            return size;
        }
    }

    public static abstract class Callback {

        public abstract boolean beforeCall(Object receiver, Object[] args) throws Exception;

        public abstract Object afterCall(Object receiver, Object[] args, Object result, Throwable throwable) throws Exception;

        public class UnHook {
            private HookME.HookRecord record;

            public UnHook(HookME.HookRecord record) {
                this.record = record;
            }

            public void unhook() {
                record.removeCallback(Callback.this);
            }
        }
    }
}
