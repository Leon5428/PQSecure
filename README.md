# PQSecure
面向 ARM64 边缘设备的实验性后量子安全通信与性能评测平台

## 第一个实验：ML-KEM-768 密钥封装

`experiments/ml_kem_768_demo.cpp` 在同一个进程中模拟服务端生成密钥、客户端封装、服务端解封装，并验证双方共享秘密一致。程序还会翻转密文中的一个比特，检查解封装结果与原共享秘密不同。所有密码学 API 返回值都被检查；私钥及共享秘密不输出，并在离开作用域时清零。

ML-KEM 对无效密文使用“隐式拒绝”：解封装可以返回成功，但产生替代秘密。因此不能把 API 返回成功当作对端身份认证或密文有效性的证明。实际协议需要后续的身份认证和 Finished 密钥确认。本实验只验证密码原语，不包含网络通信，也不直接把共享秘密用作 AES 密钥。

### Windows 原生环境

当前使用 MSYS2 UCRT64 的 GCC、liboqs、CMake 和 Ninja。请确保 `C:\msys64\ucrt64\bin` 在 PowerShell 的 PATH 中，以便找到编译器、构建工具和运行时 DLL。

在仓库根目录执行：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64
cmake --build build
.\build\ml_kem_768_demo.exe
```

如果 MSYS2 安装在其他位置，请相应调整路径。首次选择编译器后，不要在同一个构建目录中混用其他工具链。

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


