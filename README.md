# mini-ctr

A tiny container runtime written in C++20 — the core of Docker in ~1k lines of
code. It uses Linux **namespaces** for isolation, **cgroups v2** for resource
limits, `pivot_root` for the filesystem view and **capabilities** for privilege
dropping. Wrapped in the same full CI pipeline as the rest of this portfolio:
lint → build (gcc/clang) → static analysis → tests → sanitizers → fuzzing → package.

> Containers are just processes: the kernel hands them an isolated *view* of the
> system (namespaces) and a *budget* (cgroups). There is no hypervisor and no
> guest OS, which is why they start in milliseconds.

## Features

- **Namespaces** — `CLONE_NEWPID | NEWNS | NEWUTS | NEWIPC`, optional `NEWNET`.
- **Root filesystem** — private mount propagation, `pivot_root` (with `chroot`
  fallback), fresh `/proc`, `devtmpfs` `/dev` (tmpfs + device nodes fallback),
  `/dev/pts`, `/dev/shm` and a read-only `/sys`.
- **Init semantics** — the runtime acts as PID 1: it reaps zombies, sets
  `PR_SET_CHILD_SUBREAPER` and forwards signals to the container's process group.
- **Resource limits** — cgroups v2 `memory.max`, `pids.max`, `cpu.max`
  (best effort: warns and continues if cgroups v2 is unavailable).
- **Hardening** — all capabilities dropped + `PR_SET_NO_NEW_PRIVS`.
- **Tested** — GoogleTest unit tests, a privileged integration test that really
  boots a container, and a libFuzzer target for the CLI parser.

## How it works

```
parent (mini-ctr)                         child (PID 1 in the container)
─────────────────                         ──────────────────────────────
parse args → Config
create cgroup v2 ──────────────┐
clone(2) with namespace flags ─┼──────────► sethostname
                               │            setup_rootfs   (pivot_root, /proc, /dev)
                               └─ limits ──► drop_capabilities
                                             PR_SET_CHILD_SUBREAPER
                                             fork ──► command (PID 2)
waitpid ◄── exit code ─────────────────────  reap + forward signals
```

## Build & run

The code needs Linux (namespaces, cgroups). On macOS use the Docker dev image.

```bash
docker compose up -d dev
docker compose exec dev bash

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DWERROR=ON
cmake --build build

bash scripts/mkrootfs.sh ./rootfs          # minimal busybox rootfs
sudo ./build/mini-ctr run --rootfs ./rootfs --hostname demo --memory 64m /bin/sh
```

Needs `root` (or `--privileged` in a container) to create namespaces.

### Usage

```
mini-ctr run [options] <command> [args...]

  --rootfs <dir>     root filesystem to enter (default: ./rootfs)
  --hostname <name>  container hostname (default: container)
  --memory <size>    memory limit, e.g. 64m, 1g
  --pids <n>         maximum number of processes
  --cpus <n>         CPU quota in cores, e.g. 0.5
  --keep-caps        do not drop capabilities
  --no-cgroups       run without resource limits
  --host-pid         share the host PID namespace
  --net              create a new network namespace
  -h, --help
```

## Testing

```bash
ctest --test-dir build --output-on-failure            # unit + integration
ctest --test-dir build --output-on-failure -E integration   # unit only

# sanitizers
cmake -S . -B build-asan -G Ninja -DENABLE_ASAN=ON -DENABLE_UBSAN=ON
cmake --build build-asan && ctest --test-dir build-asan --output-on-failure -E integration

# fuzzing
cmake -S . -B build-fuzz -G Ninja -DBUILD_TESTING=OFF -DENABLE_FUZZING=ON -DENABLE_ASAN=ON
cmake --build build-fuzz && ./build-fuzz/fuzz/fuzz_cli
```

The integration test builds a rootfs from `busybox-static` and asserts hostname
isolation, `/proc`, `/dev` nodes, exit-code propagation and SIGTERM forwarding.
Without root it **self-skips** so the job stays green on unprivileged runners.

## CI

`.gitlab-ci.yml` mirrors every CMake switch so each stage is reproducible
locally: `lint:clang-format`, `build` (gcc/clang × Debug/Release, `-Werror`),
`static-analysis` (clang-tidy, cppcheck), `test:coverage` + `test:integration`,
`sanitizers` (ASan/UBSan/TSan), `fuzz`, and `package` (tarball + container image
published to the GitLab registry on tags).

## Project layout

| Path | Purpose |
|------|---------|
| `include/ctr/cli.hpp`, `src/cli.cpp` | Pure CLI parser + `parse_size` (fuzzable) |
| `include/ctr/config.hpp`, `src/config.cpp` | `Config` struct + validation |
| `include/ctr/namespaces.hpp`, `src/namespaces.cpp` | `clone(2)` namespace flags |
| `include/ctr/rootfs.hpp`, `src/rootfs.cpp` | `pivot_root`, `/proc`, `/dev`, `/sys` |
| `include/ctr/cgroup.hpp`, `src/cgroup.cpp` | cgroups v2 limits |
| `include/ctr/capabilities.hpp`, `src/capabilities.cpp` | drop capabilities |
| `include/ctr/container.hpp`, `src/container.cpp` | orchestration + init |
| `src/main.cpp` | entry point |
| `scripts/mkrootfs.sh` | build a busybox rootfs |
| `tests/`, `fuzz/` | unit, integration and fuzz targets |

## Limitations

- cgroups v2 is best effort: inside Docker Desktop the parent cgroup has
  processes, so `subtree_control` cannot be enabled and limits are skipped
  (logged as a warning). On a host with a delegated cgroup leaf it works.
- No user namespace, so the runtime needs real root (no rootless mode yet).
- `--net` creates an empty network namespace; there is no veth/bridge wiring.
- No OCI image pulling, overlayfs or cgroup v1 support.

## License

MIT (see `LICENSE`).
