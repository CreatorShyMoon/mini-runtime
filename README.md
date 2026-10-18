# mini-ctr

Крошечный контейнерный рантайм на C++20 — ядро Docker примерно в тысяче строк.
Изоляция построена на Linux-**namespaces**, лимиты ресурсов — на **cgroups v2**,
файловая система — через `pivot_root`, привилегии — через **capabilities**.
Вокруг — тот же полный CI-пайплайн, что и в остальном портфолио:
lint → build (gcc/clang) → статический анализ → тесты → санитайзеры → фаззинг → упаковка.

> Контейнер — это просто процесс, которому ядро выдало изолированный *вид* на систему
> (namespaces) и *бюджет* (cgroups). Здесь нет гипервизора и гостевой ОС — поэтому
> контейнеры стартуют за миллисекунды.

## Возможности

- **Namespaces** — `CLONE_NEWPID | NEWNS | NEWUTS | NEWIPC`, опционально `NEWNET`.
- **Корневая файловая система** — приватное распространение монтирований, `pivot_root`
  (с откатом на `chroot`), свежий `/proc`, `/dev` на `devtmpfs` (fallback: `tmpfs` +
  device-ноды), `/dev/pts`, `/dev/shm` и `/sys` только для чтения.
- **Семантика init** — рантайм работает как PID 1: собирает зомби, ставит
  `PR_SET_CHILD_SUBREAPER` и форвардит сигналы в process-group команды.
- **Лимиты ресурсов** — cgroups v2: `memory.max`, `pids.max`, `cpu.max`
  (best effort: если cgroups v2 недоступны, пишет предупреждение и продолжает).
- **Безопасность** — сброс всех capabilities + `PR_SET_NO_NEW_PRIVS`.
- **Протестировано** — unit-тесты GoogleTest, привилегированный интеграционный тест,
  который реально поднимает контейнер, и цель libFuzzer для парсера CLI.

## Как это работает

```
родитель (mini-ctr)                       ребёнок (PID 1 в контейнере)
───────────────────                       ───────────────────────────
parse args → Config
создать cgroup v2 ─────────────┐
clone(2) с флагами namespaces ─┼──────────► sethostname
                               │            setup_rootfs   (pivot_root, /proc, /dev)
                               └─ лимиты ──► drop_capabilities
                                             PR_SET_CHILD_SUBREAPER
                                             fork ──► команда (PID 2)
waitpid ◄── код выхода ────────────────────  reaping + форвард сигналов
```

## Сборка и запуск

Коду нужен Linux (namespaces, cgroups). На macOS используй dev-образ Docker.

```bash
docker compose up -d dev
docker compose exec dev bash

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DWERROR=ON
cmake --build build

bash scripts/mkrootfs.sh ./rootfs          # минимальный rootfs из busybox
sudo ./build/mini-ctr run --rootfs ./rootfs --hostname demo --memory 64m /bin/sh
```

Нужен `root` (или `--privileged` внутри контейнера), чтобы создавать namespaces.

### Использование

```
mini-ctr run [опции] <команда> [аргументы...]

  --rootfs <dir>     какой rootfs использовать (по умолчанию ./rootfs)
  --hostname <name>  hostname контейнера (по умолчанию container)
  --memory <size>    лимит памяти, например 64m, 1g
  --pids <n>         максимальное число процессов
  --cpus <n>         квота CPU в ядрах, например 0.5
  --keep-caps        не сбрасывать capabilities
  --no-cgroups       запустить без лимитов
  --host-pid         использовать PID-namespace хоста
  --net              создать новый network namespace
  -h, --help
```

## Тестирование

```bash
ctest --test-dir build --output-on-failure                 # unit + интеграция
ctest --test-dir build --output-on-failure -E integration  # только unit

# санитайзеры
cmake -S . -B build-asan -G Ninja -DENABLE_ASAN=ON -DENABLE_UBSAN=ON
cmake --build build-asan && ctest --test-dir build-asan --output-on-failure -E integration

# фаззинг
cmake -S . -B build-fuzz -G Ninja -DBUILD_TESTING=OFF -DENABLE_FUZZING=ON -DENABLE_ASAN=ON
cmake --build build-fuzz && ./build-fuzz/fuzz/fuzz_cli
```

Интеграционный тест собирает rootfs из `busybox-static` и проверяет изоляцию hostname,
`/proc`, device-ноды в `/dev`, проброс кода выхода и форвардинг SIGTERM.
Без root он **самовыключается**, поэтому джоба остаётся зелёной на непривилегированных раннерах.

## CI

`.gitlab-ci.yml` повторяет каждый CMake-свитч, поэтому стадии воспроизводимы локально:
`lint:clang-format`, `build` (gcc/clang × Debug/Release, `-Werror`),
`static-analysis` (clang-tidy, cppcheck), `test:coverage` + `test:integration`,
`sanitizers` (ASan/UBSan/TSan), `fuzz` и `package` (tarball + образ контейнера,
публикуемый в GitLab Registry по тегу).

## Структура проекта

| Путь | Назначение |
|------|-----------|
| `include/ctr/cli.hpp`, `src/cli.cpp` | Чистый парсер CLI + `parse_size` (фазится) |
| `include/ctr/config.hpp`, `src/config.cpp` | Структура `Config` + валидация |
| `include/ctr/namespaces.hpp`, `src/namespaces.cpp` | Флаги namespaces для `clone(2)` |
| `include/ctr/rootfs.hpp`, `src/rootfs.cpp` | `pivot_root`, `/proc`, `/dev`, `/sys` |
| `include/ctr/cgroup.hpp`, `src/cgroup.cpp` | Лимиты cgroups v2 |
| `include/ctr/capabilities.hpp`, `src/capabilities.cpp` | Сброс capabilities |
| `include/ctr/container.hpp`, `src/container.cpp` | Оркестрация + init |
| `src/main.cpp` | Точка входа |
| `scripts/mkrootfs.sh` | Сборка rootfs из busybox |
| `tests/`, `fuzz/` | Unit-, интеграционные и фаззинг-цели |

## Ограничения

- cgroups v2 работают best effort: внутри Docker Desktop в родительском cgroup есть
  процессы, поэтому `subtree_control` не включается и лимиты пропускаются
  (в лог идёт предупреждение). На хосте с делегированным leaf-cgroup всё работает.
- Нет user namespace, поэтому рантайму нужен настоящий root (rootless-режима пока нет).
- `--net` создаёт пустой network namespace; veth/bridge не настраиваются.
- Нет работы с OCI-образами, overlayfs и cgroup v1.

## Лицензия

MIT (см. `LICENSE`).
