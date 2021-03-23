package me.hook.core;

import static me.hook.core.HookME.handleBridge;

/**
 * this bridge class is for 64 bit
 */
final class BridgeJJ {

    private BridgeJJ() {}

    private static boolean boolBridge(long addr, long sp) throws Throwable {
        return (Boolean) handleBridge(addr, sp);
    }

    private static byte byteBridge(long addr, long sp) throws Throwable {
        return (Byte) handleBridge(addr, sp);
    }

    private static char charBridge(long addr, long sp) throws Throwable {
        return (Character) handleBridge(addr, sp);
    }

    private static short shortBridge(long addr, long sp) throws Throwable {
        return (Short) handleBridge(addr, sp);
    }

    private static int intBridge(long addr, long sp) throws Throwable {
        return (Integer) handleBridge(addr, sp);
    }

    private static long longBridge(long addr, long sp) throws Throwable {
        return (Long) handleBridge(addr, sp);
    }

    private static float floatBridge(long addr, long sp) throws Throwable {
        return (Float) handleBridge(addr, sp);
    }

    private static double doubleBridge(long addr, long sp) throws Throwable {
        return (Double) handleBridge(addr, sp);
    }

    private static Object objectBridge(long addr, long sp) throws Throwable {
        return handleBridge(addr, sp);
    }

    private static void voidBridge(long addr, long sp) throws Throwable {
        handleBridge(addr, sp);
    }
}
