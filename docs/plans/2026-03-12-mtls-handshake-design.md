# mTLS 握手案例设计

## 概述

在 openssl3_learn 目录开发一个完整的 mTLS（双向 TLS 认证）握手案例，展示 TLS 1.3 双向认证的完整流程。

## 技术选型

| 项目 | 选择 | 理由 |
|------|------|------|
| 证书生成 | 程序内部动态生成 | 便于演示，无需外部依赖 |
| 通信方式 | Unix Domain Socket | 本地进程间通信，简单高效 |
| TLS 版本 | TLS 1.3 | 现代推荐，更安全简洁 |
| 日志级别 | 详细版 | 展示握手细节、证书内容、密码套件 |

## TLS 1.3 mTLS 完整握手流程

```
┌─────────────────────────────────────────────────────────────────────────────────────────────┐
│                              TLS 1.3 mTLS 完整握手流程 (1-RTT)                          │
├─────────────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                          │
│   客户端 (Client)                                      服务端 (Server)                    │
│       │                                                    │                               │
│       │                                                    │  ←──── 初始化 SSL_CTX ──── │
│       │                                                    │  ←──── 加载证书和私钥 ──── │
│       │                                                    │  ←──── 加载信任的 CA ───── │
│       │                                                    │                               │
│       │  1. ClientHello                                  │                               │
│       │  ──────────────────────────────────────────────►  │                               │
│       │       • client_random (32 bytes)                 │                               │
│       │       • session_id (可选)                         │                               │
│       │       • cipher_suites []                         │                               │
│       │         - TLS_AES_256_GCM_SHA384                │                               │
│       │         - TLS_CHACHA20_POLY1305_SHA256          │                               │
│       │       • extensions []                            │                               │
│       │         - supported_versions (TLS 1.3)           │                               │
│       │         - signature_algorithms                    │                               │
│       │         - key_share (client key exchange)        │                               │
│       │         - server_name (SNI)                      │                               │
│       │         - alpn (应用协议)                        │                               │
│       │                                                    │                               │
│       │                            2. ServerHello        │                               │
│       │                            ◄────────────────────  │                               │
│       │                                • server_random    │                               │
│       │                                • cipher_suite    │                               │
│       │                                • key_share       │                               │
│       │                                  (server key exchange)                          │
│       │                                • supported_version = TLS 1.3                    │
│       │                                                    │                               │
│       │                            3. Certificate        │                               │
│       │                            ◄────────────────────  │  ←── 发送服务端证书链     │
│       │                                • certificate_list │                               │
│       │                                  - certificate (DER)                          │
│       │                                  - extensions:                                │
│       │                                    * basicConstraints: CA:FALSE                │
│       │                                    * keyUsage: digitalSignature                │
│       │                                    * extendedKeyUsage: serverAuth              │
│       │                                  - issuer_name                                │
│       │                                  - signature (CA签名)                        │
│       │                                                    │                               │
│       │                            4. CertificateVerify  │                               │
│       │                            ◄────────────────────  │  ←── 用服务端私钥签名     │
│       │                                • signature        │                               │
│       │                                  = Sign(Hash(Transcript))                       │
│       │                                  Transcript = 所有之前的握手消息               │
│       │                                                    │                               │
│       │                            5. Finished           │                               │
│       │                            ◄────────────────────  │  ←── 验证握手消息完整性     │
│       │                                • verify_data    │                               │
│       │                                  = PRF(master_secret, handshake_hash)           │
│       │                                                    │                               │
│       │                                                    │  6. CertificateRequest     │
│       │                            ─────────────────────►  │  ←── 请求客户端证书        │
│       │                                • certificate_authorities []                       │
│       │                                  - /CN=My CA/O=Company                         │
│       │                                • certificate_types []                         │
│       │                                  - ecdsa_sign                                  │
│       │                                  - rsa_sign                                    │
│       │                                • signature_algorithms []                       │
│       │                                                    │                               │
│       │  7. Certificate                                      │                           │
│       │  ──────────────────────────────────────────────►  │  ←── 发送客户端证书链     │
│       │       • certificate_list                            │                           │
│       │         - certificate (DER)                         │                           │
│       │         - extensions:                              │                           │
│       │           * basicConstraints: CA:FALSE             │                           │
│       │           * keyUsage: digitalSignature            │                           │
│       │           * extendedKeyUsage: clientAuth          │                           │
│       │         - signature (CA签名)                       │                           │
│       │                                                    │                               │
│       │  8. CertificateVerify                              │                           │
│       │  ──────────────────────────────────────────────►  │  ←── 用客户端私钥签名     │
│       │       • signature = Sign(Hash(Transcript))        │                           │
│       │                                                    │                               │
│       │  9. Finished                                      │                           │
│       │  ──────────────────────────────────────────────►  │  ←── 验证客户端证书       │
│       │       • verify_data                                │       验证握手消息完整性   │
│       │                                                    │       生成应用密钥        │
│       │                                                    │                               │
│       │═══════════════════════════════════════════════════════════════════════════════════│
│       │                     ✓ 握手完成, 进入加密通信模式                               │
│       │═══════════════════════════════════════════════════════════════════════════════════│
│       │                                                    │                               │
│       │  10. Application Data (加密)                       │                               │
│       │  ◄─────────────────────────────────────────────►  │                               │
│       │       • TLS_AES_256_GCM_SHA384 加密               │                               │
│       │       • Encrypted: "Hello from mTLS!"            │                               │
│       │                                                    │                               │
└─────────────────────────────────────────────────────────────────────────────────────────────┘
```

## CertificateRequest 详解

```
┌─────────────────────────────────────────────────────────────────────────────────────────────┐
│                              CertificateRequest 消息详解                                 │
├─────────────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                          │
│  当服务端需要客户端证书时,发送 CertificateRequest 消息                                    │
│                                                                                          │
│  ┌─────────────────────────────────────────────────────────────────────────────────┐    │
│  │  CertificateRequest 消息结构                                                    │    │
│  ├─────────────────────────────────────────────────────────────────────────────────┤    │
│  │                                                                                 │    │
│  │  certificate_request_context:                                                   │    │
│  │    - 0 (对于普通握手)                                                         │    │
│  │    - 特定上下文标识符 (用于外部 PSK 绑定)                                      │    │
│  │                                                                                 │    │
│  │  extensions []:                                                                │    │
│  │                                                                                 │    │
│  │  1. certificate_authorities:                                                  │    │
│  │     ├── DistinguishedName (DN) 列表                                            │    │
│  │     │   ├── /C=US/O=Company/CN=Enterprise CA                                 │    │
│  │     │   ├── /C=US/O=Company/CN=Department CA                                 │    │
│  │     │   └── /C=CN/O=MyOrg/CN=My CA                                          │    │
│  │     │                                                                          │    │
│  │     └── 客户端行为:                                                            │    │
│  │         - 遍历自己的证书                                                      │    │
│  │         - 检查证书签发者是否在列表中                                          │    │
│  │         - 找到匹配 → 发送该证书                                               │    │
│  │         - 未找到 → 发送空的 Certificate                                       │    │
│  │                                                                                 │    │
│  │  2. signature_algorithms:                                                     │    │
│  │     ├── ecdsa_secp256r1_sha256                                               │    │
│  │     ├── ecdsa_secp384r1_sha384                                               │    │
│  │     ├── rsa_pss_rsae_sha256                                                  │    │
│  │     └── rsa_pss_rsae_sha384                                                  │    │
│  │                                                                                 │    │
│  │  3. certificate_types (TLS 1.2 兼容):                                         │    │
│  │     ├── rsa_sign                                                              │    │
│  │     └── ecdsa_sign                                                            │    │
│  │                                                                                 │    │
│  └─────────────────────────────────────────────────────────────────────────────────┘    │
│                                                                                          │
│  客户端证书选择逻辑:                                                                    │
│                                                                                          │
│  ┌─────────────────────────────────────────────────────────────────────────────────┐    │
│  │  for each client_certificate in client_certificates:                       │    │
│  │      issuer = get_issuer(client_certificate)                                │    │
│  │      if issuer in certificate_authorities:                                 │    │
│  │          return client_certificate  ← 发送这个证书                          │    │
│  │                                                                                 │    │
│  │  return NULL  ← 发送空的 Certificate 消息                                   │    │
│  └─────────────────────────────────────────────────────────────────────────────────┘    │
│                                                                                          │
└─────────────────────────────────────────────────────────────────────────────────────────────┘
```

## 证书验证流程

```
┌─────────────────────────────────────────────────────────────────────────────────────────────┐
│                                   证书验证流程                                             │
├─────────────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                          │
│  服务端验证客户端证书:                                                                   │
│  ┌─────────────────────────────────────────────────────────────────────────────────┐    │
│  │  1. 提取客户端证书链                                                          │    │
│  │     certificate_list[0] = 客户端证书                                          │    │
│  │     certificate_list[1] = 中间CA证书 (可选)                                   │    │
│  │     ...                                                                      │    │
│  │                                                                                 │    │
│  │  2. 验证客户端证书                                                            │    │
│  │     ├── 检查时间: notBefore < now < notAfter                                 │    │
│  │     ├── 检查 keyUsage: 包含 digitalSignature                                │    │
│  │     ├── 检查 extendedKeyUsage: 包含 clientAuth                             │    │
│  │     ├── 检查 basicConstraints: CA:FALSE                                     │    │
│  │     └── 检查域名/SAN (可选)                                                  │    │
│  │                                                                                 │    │
│  │  3. 验证签名                                                                  │    │
│  │     ├── 用证书中的公钥验证签名                                                │    │
│  │     └── Hash(客户端证书) = Signature                                         │    │
│  │                                                                                 │    │
│  │  4. 验证证书链                                                                │    │
│  │     for each CA in trusted_CA_list:                                          │    │
│  │         if verify(客户端证书, CA.public_key):                                 │    │
│  │             return OK                                                         │    │
│  │                                                                                 │    │
│  │     return FAILED  ← "unknown CA" 错误                                      │    │
│  │                                                                                 │    │
│  └─────────────────────────────────────────────────────────────────────────────────┘    │
│                                                                                          │
│  验证成功条件:                                                                          │
│  • 证书未过期                                                                          │
│  • 证书链可追溯到信任的 CA                                                            │
│  • 签名验证通过                                                                       │
│  • 吊销检查通过 (CRL/OCSP) - 生产环境                                              │
│                                                                                          │
└─────────────────────────────────────────────────────────────────────────────────────────────┘
```

## 密钥派生流程

```
┌─────────────────────────────────────────────────────────────────────────────────────────────┐
│                                   TLS 1.3 密钥派生                                       │
├─────────────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                          │
│  ┌─────────────────────────────────────────────────────────────────────────────────┐    │
│  │  Early Secret (0-RTT) 或 (EC)DHE                                            │    │
│  │       │                                                                        │    │
│  │       ▼                                                                        │    │
│  │  Derive Secret(Handshake Secret)                                              │    │
│  │       │                                                                        │    │
│  │       ├──► Server Handshake Traffic Secret                                    │    │
│  │       │     └──► Server Write Key (加密服务端→客户端)                         │    │
│  │       │                                                                        │    │
│  │       └──► Client Handshake Traffic Secret                                    │    │
│  │             └──► Client Write Key (加密客户端→服务端)                         │    │
│  │                                                                            │    │
│  │       ▼                                                                        │    │
│  │  Master Secret                                                              │    │
│  │       │                                                                        │    │
│  │       ├──► Server Application Traffic Secret                                   │    │
│  │       │     └──► Server Write Key (应用数据加密)                             │    │
│  │       │                                                                        │    │
│  │       └──► Client Application Traffic Secret                                   │    │
│  │             └──► Client Write Key (应用数据加密)                             │    │
│  │                                                                            │    │
│  └─────────────────────────────────────────────────────────────────────────────────┘    │
│                                                                                          │
│  HKDF 派生:                                                                             │
│  • Early Secret: HKDF-Extract(0, 0) 或 HKDF-Extract(PSK, 0)                          │
│  • Handshake Secret: HKDF-Extract(Early Secret, (EC)DHE)                               │
│  • Master Secret: HKDF-Extract(Handshake Secret, 0)                                     │
│  • Application Traffic Secrets: HKDF-Expand-Label(Master Secret, ...)                   │
│                                                                                          │
└─────────────────────────────────────────────────────────────────────────────────────────────┘
```

## 架构设计

### 模块划分

```
┌─────────────────────────────────────────────────────────┐
│                    mTLS 握手案例                          │
├─────────────────────────────────────────────────────────┤
│  1. 证书管理模块                                          │
│     ├── generate_ca()           → 生成自签名 CA 证书        │
│     ├── generate_server_cert()  → 使用 CA 签发服务器证书    │
│     └── generate_client_cert()  → 使用 CA 签发客户端证书    │
│                                                         │
│  2. SSL_CTX 配置模块                                     │
│     ├── create_server_ctx()    → 配置服务端 SSL_CTX      │
│     ├── create_client_ctx()    → 配置客户端 SSL_CTX      │
│     └── load_cert_verify()    → 加载证书并设置验证      │
│                                                         │
│  3. 通信模块（Unix Domain Socket）                        │
│     ├── create_server_socket()  → 创建并监听 socket       │
│     ├── connect_client_socket() → 客户端连接              │
│     └── do_handshake()          → 执行 TLS 握手          │
│                                                         │
│  4. 日志输出模块                                          │
│     ├── print_ssl_info()       → 打印 SSL 连接信息       │
│     ├── print_cert_info()       → 打印证书详细信息        │
│     └── print_trusted_ca_list() → 打印信任的 CA 列表      │
└─────────────────────────────────────────────────────────┘
```

### 证书生成流程

1. **CA 证书**：生成 RSA 2048 位密钥，自签名生成 CA 证书，包含 `basicConstraints=CA:TRUE`
2. **服务器证书**：使用 CA 密钥签发，CN=localhost，包含 `keyUsage=digitalSignature`
3. **客户端证书**：使用 CA 密钥签发，CN=Client，包含 `keyUsage=digitalSignature`

### SSL 配置

**服务端**：
- TLS 1.3
- 加载服务器证书 + 私钥
- 加载 CA 证书到信任存储
- `SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT`

**客户端**：
- TLS 1.3
- 加载客户端证书 + 私钥
- 加载 CA 证书到信任存储
- `SSL_VERIFY_PEER`

## 文件结构

```
openssl3_learn/
├── xmake.lua              # 添加 mTLS 目标
├── mTLS_test.c            # 主程序
└── docs/plans/
    └── 2026-03-12-mtls-handshake-design.md
```

## 预期输出

程序运行后将展示：
1. 证书生成过程 (CA、Server、Client)
2. 信任 CA 列表
3. 服务端启动并监听
4. 客户端发起连接
5. 双向 TLS 握手全过程
6. 握手成功后交换加密数据
