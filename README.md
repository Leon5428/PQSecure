# PQSecure
面向 ARM64 边缘设备的实验性后量子安全通信与性能评测平台

## 第一个实验：ML-KEM-768 密钥封装

`experiments/ml_kem_768_demo.cpp` 在同一个进程中模拟服务端生成密钥、客户端封装、服务端解封装，并验证双方共享秘密一致。程序还会翻转密文中的一个比特，检查解封装结果与原共享秘密不同。所有密码学 API 返回值都被检查；私钥及共享秘密不输出，并在离开作用域时清零。

ML-KEM 对无效密文使用“隐式拒绝”：解封装可以返回成功，但产生替代秘密。因此不能把 API 返回成功当作对端身份认证或密文有效性的证明。实际协议需要后续的身份认证和 Finished 密钥确认。本实验只验证密码原语，不包含网络通信，也不直接把共享秘密用作 AES 密钥。

### Windows 原生环境

双方各自在本机安装 MSYS2 UCRT64 的 GCC、liboqs、CMake、Ninja 和 GDB，安装目录可以不同。在 MSYS2 UCRT64 终端安装这些包：

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-liboqs mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-gdb
```

将各自实际的 `<MSYS2安装目录>\ucrt64\bin` 加入 Windows 用户 PATH，并完全退出再重新打开 VS Code。共享配置通过 PATH 查找工具和运行时 DLL，不包含任何人的安装目录。应优先使用同一 UCRT64 环境中的工具和库，避免混用其他 GCC、MSVC 或其他架构的库。

在 PowerShell 中使用 `Get-Command cmake,g++,ninja,gdb` 检查命中的工具位置。VS Code 调试还需要安装 Microsoft C/C++ 扩展。

在仓库根目录执行：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build build
.\build\ml_kem_768_demo.exe
```

使用同一 UCRT64 安装中的 CMake 时，会搜索其安装前缀下的 liboqs 配置。如果 liboqs 单独安装在其他目录，请在自己的用户环境变量 `CMAKE_PREFIX_PATH` 中填写它的安装前缀（包含 `include` 和 `lib` 的目录）；也可在命令行用 `-Dliboqs_DIR=...` 指定包含 `liboqsConfig.cmake` 的目录。个人路径无需提交到仓库。

`build/` 中的 CMake 缓存会保存本机绝对路径，因此每人应独立构建，不要共享或提交该目录。移动项目、移动依赖或更换编译器后，需要重新生成干净的构建目录。

VS Code 中选择 `PQSecure: ML-KEM-768 demo`，按 F5 即可配置、构建并调试。构建任务会构建所有默认目标；以后增加可执行目标时无需为每个目标新增构建任务，但调试新程序仍需对应的启动配置。该调试配置使用 GDB；其他工具链需要匹配的调试器配置。

预期输出：

```text
Algorithm: ML-KEM-768
Public key: 1184 bytes
Secret key: 2400 bytes
Ciphertext: 1088 bytes
Shared secret: 32 bytes
[PASS] Server key generation
[PASS] Client encapsulation
[PASS] Server decapsulation
[PASS] Shared secrets match
[PASS] Tampered ciphertext produces a different shared secret
```

运行自动检查（包括正常流程及篡改密文检查）：

```powershell
ctest --test-dir build --output-on-failure
```

任何验证失败都会返回非零退出码。上述负向检查是一个固定长度密文的单比特篡改案例，不代表完整的协议安全验证。


