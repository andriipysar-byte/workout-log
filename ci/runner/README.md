# CI runner

Every job in `.github/workflows/ci.yml` runs on a self-hosted runner labelled
`workout-log`. This directory builds that runner as a Docker image with the
whole toolchain inside it: clang 18, GCC 13, clang-tidy, CMake, Ninja, and
Qt 6.4 with Svg. The host needs only Docker.

Each container registers an *ephemeral* runner, takes one job and exits. Docker
restarts it with a fresh registration, so no build tree or checkout survives
from one job into the next.

## Set up (once, on the Linux machine)

1. Install Docker Engine with the compose plugin.
2. Lower the kernel's mmap randomisation. ThreadSanitizer's shadow memory does
   not fit around the default on recent kernels, and the `sanitize (tsan)` job
   aborts at start-up until this is set. It is a host setting, because a
   container cannot change it:

   ```sh
   echo 'vm.mmap_rnd_bits = 28' | sudo tee /etc/sysctl.d/60-workout-log-tsan.conf
   sudo sysctl --system
   ```

3. Give the runners a token:

   ```sh
   cp ci/runner/.env.example ci/runner/.env
   ```

   Set `ACCESS_TOKEN` in that file to a fine-grained personal access token that
   covers only this repository and has **Administration: Read and write**. The
   containers use it to create their own registration tokens, which expire after
   an hour.

4. Start them:

   ```sh
   docker compose -f ci/runner/compose.yaml up -d --build
   ```

The runners appear under *Settings → Actions → Runners*. `restart:
unless-stopped` brings them back after a reboot.

## Operate

- Logs: `docker compose -f ci/runner/compose.yaml logs -f`
- Stop: `docker compose -f ci/runner/compose.yaml down`. Each runner
  deregisters on the way out.
- Update the toolchain or the runner: rerun `up -d --build`. The image fetches
  the latest runner release when it is built.
- Parallelism: set `RUNNERS` in `.env`.

## Security

The repository is public. Every job in the workflow skips pull requests from
forks, so a stranger's code never runs on this machine. Keep that `if:` on any
job you add.
