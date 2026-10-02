# AGENTS.md

## 1. Project Overview

PQSecure is an experimental post-quantum secure communication and benchmarking framework.

The project focuses on building a real client-server communication system by composing standardized cryptographic primitives rather than merely demonstrating individual algorithms.

The primary research question is:

> How can standardized post-quantum cryptographic primitives be composed into a working secure communication protocol, and what computational and communication costs does post-quantum migration introduce on x86-64 and ARM64 edge platforms?

PQSecure is intended for:

- cryptographic engineering research;
- post-quantum cryptography experiments;
- protocol engineering;
- ARM64 performance evaluation;
- reproducible benchmarking;
- educational and research use.

PQSecure is NOT intended to be advertised as a production replacement for TLS.

Use terms such as:

- experimental framework;
- research prototype;
- post-quantum secure communication prototype.

Do not claim that the protocol is production-ready or formally proven secure.

## 2. Target Platforms

The main development and experimental platforms are:

### Client

- Windows / WSL
- x86-64

### Server

- Orange Pi 5 Pro
- ARM64 / AArch64
- Ubuntu 22.04
- hostname: `cryptolab`

Current direct network topology:

```text
Windows / WSL
x86-64
192.168.50.1
      |
      | Ethernet
      |
192.168.50.2
Orange Pi 5 Pro
ARM64
cryptolab
```

The Orange Pi acts as the primary PQSecure server and ARM64 benchmark node.

## 3. High-Level Architecture

PQSecure is organized conceptually into four layers:

```text
+----------------------------------+
|           Application            |
|     Message / File Transfer      |
+----------------------------------+
|            Protocol              |
| Handshake / Session / Replay     |
+----------------------------------+
|          Cryptography            |
| ML-KEM / ML-DSA / KDF / AEAD     |
+----------------------------------+
|            Platform              |
|      x86-64 <-> ARM64            |
+----------------------------------+
```

Keep these layers separated in the implementation.

Avoid mixing:

- socket handling with cryptographic implementation;
- protocol serialization with network I/O;
- benchmarking code with production-path protocol logic;
- application logic with low-level crypto wrappers.

## 4. Core Cryptographic Design

The intended cryptographic responsibilities are:

### ML-KEM

Purpose:

- establish a shared secret between client and server.

ML-KEM is NOT used to directly encrypt application data or files.

Planned parameter sets:

- ML-KEM-512
- ML-KEM-768
- ML-KEM-1024

The initial/default target is:

- ML-KEM-768

### ML-DSA

Purpose:

- authenticate the server;
- sign handshake data;
- protect the authenticity of handshake parameters.

Planned parameter sets:

- ML-DSA-44
- ML-DSA-65
- ML-DSA-87

The initial/default target is:

- ML-DSA-65

### AEAD

Purpose:

- protect application data after the handshake.

Initial/default algorithm:

- AES-256-GCM

Possible future algorithm:

- ChaCha20-Poly1305

### KDF

The shared secret produced by ML-KEM must NOT be used directly as an AES key.

Derive session key material using a KDF.

Initial design:

- HKDF-SHA256

The KDF should derive independent material such as:

- client-to-server write key;
- server-to-client write key;
- client IV/nonce material;
- server IV/nonce material;
- Finished authentication key.

Client-to-server and server-to-client traffic should use separate keys.

## 5. External Cryptographic Libraries

During the first major development phase, DO NOT implement ML-KEM or ML-DSA from scratch.

Use:

- liboqs for ML-KEM and ML-DSA;
- OpenSSL where appropriate for symmetric cryptography, hashing, HKDF, and classical algorithms.

The purpose of PQSecure v1 is primarily to study:

- protocol composition;
- network engineering;
- authentication;
- session management;
- secure record handling;
- benchmarking;
- cross-platform performance.

Do not prematurely spend development effort implementing:

- NTT;
- lattice polynomial arithmetic;
- ML-KEM sampling internals;
- ML-KEM compression internals;
- custom SHAKE implementations.

Low-level ML-KEM optimization may be explored later as a separate research phase.

## 6. Protocol Direction

The initial protocol is referred to as:

`PQSecure Protocol v1`

A simplified handshake is:

```text
Client                                  Server

  |                                       |
  | -------- ClientHello ---------------> |
  |                                       |
  | <-------- ServerHello --------------- |
  |          ML-KEM public key            |
  |          ML-DSA signature             |
  |                                       |
  | ------- KEMCiphertext --------------> |
  |                                       |
  |      derive shared secret             |
  |                                       |
  | <=========== Finished ==============> |
  |                                       |
  | ======== encrypted channel =========  |
```

This protocol format is still experimental.

Do not treat the current wire format as permanently frozen unless explicitly documented otherwise.

## 7. ClientHello

The protocol is expected to support a ClientHello containing information similar to:

```text
ClientHello {
    version

    supported_kem
    supported_signature
    supported_aead

    client_nonce
}
```

The current design assumes a random client nonce.

The protocol should eventually support algorithm negotiation.

## 8. ServerHello

The server selects protocol algorithms and creates handshake-specific state.

A conceptual ServerHello contains:

```text
ServerHello {
    version

    selected_kem
    selected_signature
    selected_aead

    client_nonce
    server_nonce

    kem_public_key
}
```

The server signs the relevant handshake transcript using ML-DSA before the client accepts the ML-KEM public key.

## 9. Server Authentication

The client is expected to know or trust the server's ML-DSA public key before establishing the session.

The server signs handshake data.

The client must verify the ML-DSA signature before continuing with ML-KEM encapsulation.

Conceptually:

```text
signature = ML-DSA-Sign(
    ClientHello || ServerHello
)
```

If verification fails:

```text
ABORT
```

Never silently continue after an authentication failure.

## 10. Key Establishment

After authenticating the server, the client performs ML-KEM encapsulation using the server's ML-KEM public key.

Conceptually:

```text
ciphertext, shared_secret =
    Encaps(kem_public_key)
```

The client sends the KEM ciphertext.

The server performs:

```text
shared_secret =
    Decaps(kem_private_key, ciphertext)
```

Both sides should then possess the same shared secret.

That shared secret must be processed through the KDF before being used for application traffic.

## 11. Finished Messages

The protocol should include a Finished stage after key derivation.

The purpose is to confirm that:

- both parties derived compatible key material;
- both parties observed the same handshake transcript;
- the handshake has not been silently modified.

Finished authentication should depend on:

- derived Finished key material;
- the handshake transcript.

The exact encoding may evolve as the protocol implementation matures.

## 12. Record Layer

Application data must be transmitted through an authenticated encrypted record layer.

A record should eventually include metadata similar to:

```text
Version
Type
Flags
Length
Sequence Number
Payload
```

Current conceptual header design:

```text
Version      1 byte
Type         1 byte
Flags        2 bytes
Length       4 bytes
Sequence     8 bytes
```

Possible message types include:

```text
0x01 ClientHello
0x02 ServerHello
0x03 KEMCiphertext
0x04 Finished

0x10 ApplicationData
0x11 FileData
0x12 KeyUpdate

0xFF Error
```

These numeric values are experimental and may change.

## 13. Replay Protection

Authenticated encryption alone does not prevent replay of valid encrypted records.

Each application record must eventually include a sequence number.

The sequence number should be authenticated as part of AEAD associated data.

Each endpoint must track expected sequence state.

Repeated or invalid sequence numbers must be rejected.

Never silently accept replayed records.

## 14. Networking

The initial transport is TCP.

The networking layer must correctly handle TCP as a byte stream.

Do not assume:

```text
one send() == one recv()
```

Implement proper framing and exact-length reads.

Network code must correctly handle:

- partial reads;
- partial writes;
- connection termination;
- invalid packet lengths;
- malformed messages;
- timeout/error conditions where applicable.

Keep transport code independent from cryptographic code.

## 15. Serialization

Protocol messages must have deterministic serialization.

Do not transmit raw C/C++ structs directly across the network.

Explicitly define:

- integer widths;
- byte order;
- field lengths;
- variable-length encoding;
- maximum accepted sizes.

Use network byte order or another explicitly documented canonical byte order.

Always validate length fields before allocating or reading large buffers.

## 16. File Transfer

A later development stage will support encrypted file transfer.

Conceptual CLI:

```bash
pqsecure-client send test.pdf
```

File transfer should eventually provide:

- encrypted transport;
- chunked transfer;
- integrity verification;
- correct reconstruction;
- useful transfer statistics.

Do not implement file transfer before the core encrypted record layer is stable.

## 17. Benchmarking

Benchmarking is a first-class feature of PQSecure.

Do not reduce benchmark work to printing a single elapsed time.

The project should eventually measure:

### Cryptographic primitives

- ML-KEM KeyGen
- ML-KEM Encapsulation
- ML-KEM Decapsulation
- ML-DSA KeyGen
- ML-DSA Sign
- ML-DSA Verify

### Protocol

- total handshake latency

### Network

- bytes sent
- bytes received
- handshake traffic

### Application

- encrypted throughput
- file transfer throughput

### System

- CPU usage
- memory usage

Benchmark output should be machine-readable where possible.

## 18. Benchmark Platforms

Important comparisons include:

```text
x86-64
vs
ARM64
```

The Orange Pi 5 Pro also allows future comparisons between:

```text
Cortex-A76
vs
Cortex-A55
```

A future experiment may compare:

```text
x86-64
vs
Cortex-A76
vs
Cortex-A55
```

Benchmark methodology must be documented so results are reproducible.

## 19. Future Hybrid Key Exchange

PQSecure v2 may add hybrid key exchange.

Planned comparison modes:

```text
Mode A:
X25519

Mode B:
ML-KEM-768

Mode C:
X25519 + ML-KEM-768
```

A hybrid design should combine the classical and post-quantum shared secrets through a KDF.

Do not simply concatenate secrets and use the result directly as an encryption key.

## 20. Development Roadmap

Follow incremental development.

Do not attempt to implement the entire system in one change.

### Stage 1 — PQC Hello World

Implement basic liboqs wrappers.

Verify:

```text
ML-KEM:
KeyGen -> Encaps -> Decaps
```

The client-side and server-side shared secrets must match.

Also verify:

```text
ML-DSA:
KeyGen -> Sign -> Verify
```

### Stage 2 — Plain TCP

Implement a minimal TCP client/server without cryptography.

Example:

```text
Client -> "hello"
Server -> "hello client"
```

### Stage 3 — TCP + ML-KEM

Transfer ML-KEM material over TCP.

Verify that both peers establish the same shared secret.

### Stage 4 — Encrypted Hello World

Add:

- HKDF;
- AES-256-GCM.

Transmit an authenticated encrypted message.

This represents approximately:

`PQSecure v0.1`

### Stage 5 — Server Authentication

Add ML-DSA handshake authentication.

This represents approximately:

`PQSecure v0.2`

### Stage 6 — Complete Basic Handshake

Add:

- sequence numbers;
- replay protection;
- Finished messages;
- stronger error handling.

This represents approximately:

`PQSecure v0.3`

### Stage 7 — File Transfer

Implement encrypted file transfer.

### Stage 8 — Benchmark Suite

Implement reproducible cross-platform benchmarks.

This represents approximately:

`PQSecure v1.0`

### Stage 9 — Hybrid PQC

Add:

- X25519;
- classical mode;
- post-quantum mode;
- hybrid mode.

This represents approximately:

`PQSecure v2.0`

## 21. Intended Repository Structure

The intended project layout is approximately:

```text
PQSecure/
|
|-- CMakeLists.txt
|-- README.md
|-- LICENSE
|
|-- include/
|   `-- pqsecure/
|
|-- src/
|   |
|   |-- client/
|   |   |-- client.cpp
|   |   `-- client_session.cpp
|   |
|   |-- server/
|   |   |-- server.cpp
|   |   `-- server_session.cpp
|   |
|   |-- crypto/
|   |   |-- kem.cpp
|   |   |-- signature.cpp
|   |   |-- aead.cpp
|   |   |-- hkdf.cpp
|   |   `-- random.cpp
|   |
|   |-- protocol/
|   |   |-- handshake.cpp
|   |   |-- record.cpp
|   |   `-- message.cpp
|   |
|   `-- network/
|       |-- socket.cpp
|       `-- transport.cpp
|
|-- benchmark/
|   |-- kem_bench.cpp
|   |-- signature_bench.cpp
|   |-- handshake_bench.cpp
|   `-- throughput_bench.cpp
|
|-- tests/
|   |-- test_kem.cpp
|   |-- test_signature.cpp
|   |-- test_protocol.cpp
|   `-- test_replay.cpp
|
|-- scripts/
|   |-- run_benchmark.py
|   `-- plot_results.py
|
|-- results/
|
`-- docs/
    |-- protocol.md
    |-- threat-model.md
    |-- benchmark.md
    `-- architecture.md
```

Do not create all files prematurely.

Add directories and files when their corresponding development stage begins.

## 22. Code Organization Rules

Prefer small modules with clear responsibilities.

Suggested responsibilities:

### `crypto/`

Only cryptographic wrappers and crypto-related utility logic.

Examples:

- KEM wrapper;
- signature wrapper;
- AEAD wrapper;
- KDF wrapper;
- randomness.

Do not put socket operations here.

### `network/`

Transport and socket operations.

Do not perform cryptographic policy decisions here.

### `protocol/`

Protocol state machines, messages, records, serialization, and handshake logic.

This layer coordinates crypto and transport.

### `client/` and `server/`

Application entry points and session orchestration.

Avoid duplicating cryptographic code between client and server.

## 23. Security Rules

Security-sensitive code must be treated conservatively.

Always:

- check return values from cryptographic APIs;
- validate incoming message lengths;
- reject malformed protocol messages;
- fail closed on authentication errors;
- avoid exposing private keys;
- avoid logging secret key material;
- avoid logging shared secrets;
- avoid logging session keys;
- avoid committing generated private keys.

Never intentionally print secrets merely for debugging unless explicitly working in a temporary local test and the output cannot be committed.

Remove such debugging before completing the task.

## 24. Secret and Key Management

Never commit:

```text
*.key
*.pem
private/
secrets/
```

or any equivalent real private-key material.

Test keys should either:

- be generated at runtime;
- live in explicitly documented test fixtures;
- contain no real secrets.

Never hardcode production-style secret keys in source code.

## 25. Error Handling

Security failures must be explicit.

Examples:

- signature verification failure;
- AEAD authentication failure;
- malformed packet;
- invalid sequence number;
- unsupported algorithm;
- KEM failure.

Do not silently ignore these conditions.

Do not continue a session after cryptographic authentication failure.

## 26. Testing Requirements

Whenever adding a security-sensitive component, add or update tests.

Important test categories include:

### ML-KEM

- successful shared-secret agreement;
- corrupted ciphertext behavior.

### ML-DSA

- successful signature verification;
- modified message rejection;
- modified signature rejection.

### AEAD

- successful encryption/decryption;
- modified ciphertext rejection;
- modified authentication tag rejection.

### Protocol

- valid handshake;
- malformed messages;
- invalid lengths;
- unsupported algorithms.

### Replay

- valid increasing sequence numbers;
- repeated record rejection.

Tests should verify failure paths, not only successful paths.

## 27. Benchmark Integrity

Benchmark code must not silently change protocol behavior.

Benchmarking must be reproducible.

Record relevant information such as:

- platform;
- architecture;
- algorithm;
- parameter set;
- compiler;
- compiler flags where relevant;
- iteration count.

Avoid drawing conclusions from a single measurement.

## 28. Build System

Use CMake as the primary build system.

Ninja may be used as the build backend.

Keep platform-specific code isolated where practical.

The project should remain buildable on both:

- x86-64 Linux/WSL;
- ARM64 Linux.

Do not introduce unnecessary platform dependencies.

## 29. Coding Style

Prefer:

- clear names;
- small functions;
- explicit ownership;
- RAII;
- standard containers;
- minimal global state.

Avoid:

- unnecessary macros;
- raw memory management when standard C++ abstractions are sufficient;
- hidden global cryptographic state;
- duplicated client/server logic;
- premature optimization.

Security and clarity are more important than clever code.

## 30. Comments

Comments should explain:

- why a security decision exists;
- protocol invariants;
- non-obvious cryptographic requirements;
- assumptions and constraints.

Avoid comments that merely restate obvious code.

## 31. Documentation

Important design decisions should eventually be documented under:

```text
docs/
```

Especially:

```text
docs/protocol.md
docs/threat-model.md
docs/architecture.md
docs/benchmark.md
```

When a protocol-level decision changes, update documentation together with code.

## 32. Threat Model

The initial threat model considers an attacker who may:

- observe network traffic;
- modify network traffic;
- inject packets;
- replay previously valid packets;
- impersonate a server without possessing the trusted ML-DSA private key.

The protocol should use:

```text
ML-KEM
-> session secret establishment

ML-DSA
-> server authentication

HKDF
-> session key derivation

AEAD
-> confidentiality + integrity

sequence numbers
-> replay protection

Finished messages
-> handshake confirmation
```

Do not assume that the network is trusted.

## 33. Scope Control

Before implementing a feature, determine whether it belongs to the current development stage.

Do NOT add unrelated features simply because they are interesting.

In particular, avoid prematurely adding:

- GUI applications;
- web dashboards;
- databases;
- blockchain components;
- zero-knowledge proofs;
- FHE;
- custom lattice implementations;
- complex distributed architecture.

Keep PQSecure focused on post-quantum secure communication and benchmarking.

## 34. Agent Behavior

When modifying this repository:

1. Inspect existing code before proposing structural changes.
2. Preserve existing working behavior unless the task explicitly changes it.
3. Prefer incremental changes.
4. Do not rewrite large working modules without a clear reason.
5. Explain security-sensitive design changes.
6. Add tests when modifying protocol or cryptographic behavior.
7. Keep x86-64 and ARM64 compatibility in mind.
8. Do not invent undocumented protocol requirements.
9. If a security decision is ambiguous, surface the ambiguity instead of silently choosing a dangerous default.
10. Do not claim security properties that have not been established.

## 35. Definition of Done

A task is not complete merely because it compiles.

For ordinary implementation tasks, completion should generally mean:

```text
build succeeds
+
relevant tests pass
+
error paths are handled
+
no secrets are exposed
+
documentation is updated when required
```

For protocol changes:

```text
implementation
+
serialization/parsing
+
positive tests
+
negative tests
+
documentation
```

For benchmark changes:

```text
measurement implementation
+
repeatability
+
machine-readable output where practical
+
documented methodology
```

## 36. Long-Term Direction

PQSecure may later integrate with a broader personal cryptographic engineering environment called CryptoLab.

Related future research directions may include:

- LightCryptoBench;
- ARM cryptographic optimization;
- AArch64 / NEON optimization;
- constant-time implementation analysis;
- classical vs post-quantum vs hybrid comparisons.

However, these should not distract from completing PQSecure v1 first.

## 37. Current Priority

The highest priority is to build a correct minimal system incrementally.

Current development order:

```text
liboqs basic experiments
        |
        v
ML-KEM + ML-DSA wrappers
        |
        v
basic TCP client/server
        |
        v
TCP + ML-KEM
        |
        v
HKDF + AES-GCM
        |
        v
ML-DSA authentication
        |
        v
record layer + replay protection
        |
        v
file transfer
        |
        v
benchmark suite
```

Prefer completing each layer before expanding the project horizontally.

Correctness first.

Security requirements apply from the beginning.

Measurement third.

Optimization comes only after the system is correct and measurable.
