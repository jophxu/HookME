本人对Android hook感兴趣，因此想动手写一个hook框架
支持 Android 8 ~ Android 10
这边只用了入口替换因此只支持 android 8 之后的版本

由于本人对于汇编,art,编译原理都不熟，走过许多弯路。最终学习各位前辈的框架花了许多时间才折腾出一个。

虽然借鉴了很多，但也有一些自己的亮点。现在开源出来，做个纪念。

与其他框架的差异
说是差异也只是一些小改进
1. 用shorty代替来保存参数类型

2. 参数的读取
我看大部分框架都是依次读取参数的，我这边是把参数一次性读取到一个数组中。
结合shorty获取参数数组，减少了native代码的调用应该多少能提高一些效率
hook_me_getArgArray

3. 固定数量的跳板方法
10个跳板针对10种不同的返回类型。
initTrampoline

4. x86, x86-64, arm, arm-64 支持
借助YAHFA 中的二进制代码，稍作加工居然很轻松的支持了这4中芯片，这个我也没想到


test App https://github.com/jophxu/HookMeDemo

自然稳定性还没发保证，应该还存在许多严重问题。慎用！！！

还有很多事情要做, 但是由于最近比较忙暂时不往下深入了。
1. hide api的限制
2. native 的hook
3. 如何hook framework 等
...



感谢
YAHFA    第一个我能看懂的框架，感觉对我帮助也最大
SandHook 对我帮助很大
Pine     对我帮助很大，test app 写的非常棒，因此照搬了过来。谢谢
Epic



