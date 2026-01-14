# LD_PRELOAD 库注入

```shell
❯ xmake run ld_preload_main
2

❯ LD_PRELOAD=/run/media/black/Data/Documents/c/build/linux/x86_64/debug/libld_preload_hook.so xmake run ld_preload_main
4
```
