TCP_KEEPIDLE (及整个 Keepalive 机制)： 在连接空闲时主动探测对端是否存活。它是检测机制。
TCP_USER_TIMEOUT： 定义了应用层对数据传输失败（表现为无 ACK）的最大容忍时间。它是连接存活的最终判决和资源回收的触发点。它涵盖了所有未确认数据的情况，而不仅仅是空闲连接。



[连接建立] --(应用数据发送/接收)--> [正常通信]
|
|  (连接空闲开始)
|  V
|  [等待 TCP_KEEPIDLE 秒] --(空闲时间达到 KEEPIDLE)--> [发送第一个 Keepalive 探测包]
|                                                               |
|                                                               V
|                                               [等待 TCP_KEEPINTVL 秒] --> [发送下一个探测包] (重复 KEEPCNT 次)
|                                                                 |
|                                                                 | (所有探测无响应)
|                                                                 V
|                                                      [Keepalive 判定连接死亡]
|
|  (或任何时候有未确认数据)
|  V
|  [未确认数据包发送时刻] --(启动 TCP_USER_TIMEOUT 计时器)-->
|                                                               |
|                                                               | (在 USER_TIMEOUT 毫秒内)
|                                                               |   - 收到 ACK？ --> 重置计时器，继续通信
|                                                               |   - 收到有效数据？ --> 重置计时器，继续通信
|                                                               |   - Keepalive ACK？ --> 重置计时器，连接保持但空闲
|                                                               |
|                                                               V (超时发生)
|                                                     [内核强制关闭连接 (ETIMEDOUT)]
|
V
[连接关闭]

明确目的： 想检测空闲连接失效？调 TCP_KEEPIDLE/KEEPINTVL/KEEPCNT。想控制应用发送数据后等待 ACK 的最长时间？调 TCP_USER_TIMEOUT。
协同设置： 如果同时使用两者，务必确保 TCP_USER_TIMEOUT >= (TCP_KEEPIDLE + (TCP_KEEPINTVL * TCP_KEEPCNT)) * 1000。这是最常见的推荐配置方式，让 Keepalive 有足够时间探测，最后由 USER_TIMEOUT 或 Keepalive 失败（触发 USER_TIMEOUT）来关闭连接。
避免冲突： 不要设置 TCP_USER_TIMEOUT 小于 TCP_KEEPIDLE（除非你有特殊理由希望尽早超时而不等 Keepalive）。
应用感知： TCP_USER_TIMEOUT 直接关系到应用 send/write 系统调用的行为和错误返回 (ETIMEDOUT)，对应用逻辑设计影响更大。
系统默认： 了解你的操作系统和应用程序的默认设置，必要时显式配置。
