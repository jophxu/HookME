package me.hook.core;

import static me.hook.core.HookME.handleBridge;

/**
 * this bridge class is for 64 bit
 */
final class BridgeII {

    private static long ADDRESS_MASK = 0x00000000FFFFFFFFl;

    private BridgeII() {}

    private static boolean boolBridge(int addr, int sp) throws Throwable {
        return (Boolean) handleBridge(ADDRESS_MASK & addr, ADDRESS_MASK & sp);
    }

    private static byte byteBridge(int addr, int sp) throws Throwable {
        return (Byte) handleBridge(ADDRESS_MASK & addr, ADDRESS_MASK & sp);
    }

    private static char charBridge(int addr, int sp) throws Throwable {
        return (Character) handleBridge(ADDRESS_MASK & addr, ADDRESS_MASK & sp);
    }

    private static short shortBridge(int addr, int sp) throws Throwable {
        return (Short) handleBridge(ADDRESS_MASK & addr, ADDRESS_MASK & sp);
    }

    private static int intBridge(int addr, int sp) throws Throwable {
        return (Integer) handleBridge(ADDRESS_MASK & addr, ADDRESS_MASK & sp);
    }

    private static long longBridge(int addr, int sp) throws Throwable {
        return (Long) handleBridge(ADDRESS_MASK & addr, ADDRESS_MASK & sp);
    }

    private static float floatBridge(int addr, int sp) throws Throwable {
        return (Float) handleBridge(ADDRESS_MASK & addr, ADDRESS_MASK & sp);
    }

    private static double doubleBridge(int addr, int sp) throws Throwable {
        return (Double) handleBridge(ADDRESS_MASK & addr, ADDRESS_MASK & sp);
    }

    private static Object objectBridge(int addr, int sp) throws Throwable {
        return handleBridge(ADDRESS_MASK & addr, ADDRESS_MASK & sp);
    }

    private static void voidBridge(int addr, int sp) throws Throwable {
        handleBridge(ADDRESS_MASK & addr, ADDRESS_MASK & sp);
    }
}
