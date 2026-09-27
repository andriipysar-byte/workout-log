# The CI agent: Ubuntu 24.04 with every toolchain the workflow's jobs use, and
# a GitHub Actions runner that serves one job per container start.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential clang clang-tidy cmake ninja-build git curl jq ca-certificates \
      qt6-base-dev qt6-svg-dev libgl1-mesa-dev nlohmann-json3-dev \
 && rm -rf /var/lib/apt/lists/*

# Empty picks the latest release at build time; rebuild the image to update.
ARG RUNNER_VERSION=""
WORKDIR /home/ubuntu/runner
RUN case "$(dpkg --print-architecture)" in amd64) arch=x64 ;; arm64) arch=arm64 ;; *) exit 1 ;; esac \
 && version="${RUNNER_VERSION:-$(curl -fsSL https://api.github.com/repos/actions/runner/releases/latest | jq -r .tag_name | sed 's/^v//')}" \
 && curl -fsSL "https://github.com/actions/runner/releases/download/v${version}/actions-runner-linux-${arch}-${version}.tar.gz" | tar xz \
 && ./bin/installdependencies.sh \
 && rm -rf /var/lib/apt/lists/* \
 && chown -R ubuntu:ubuntu /home/ubuntu

COPY --chmod=755 entrypoint.sh /usr/local/bin/entrypoint.sh
USER ubuntu
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
