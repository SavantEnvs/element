# syntax=docker/dockerfile:1.7
# Commit image for kushview/element (modular audio plugin host; C++20, CMake, JUCE, Lua/sol2).
FROM ghcr.io/savantenvs/base:20261023

ARG COMMIT
ARG REPO_URL
ARG SOURCE_DATE_EPOCH
ARG SANITIZER_FLAGS
ARG COVERAGE_FLAGS=
ARG MAYHEM_JOBS=

# JUCE's Linux build needs ALSA/X11/freetype/fontconfig/curl/GL headers; the tests use header-only Boost.Test.
USER root
RUN [ -n "$SOURCE_DATE_EPOCH" ] && repro-sources-list.sh || true ; \
    apt-get update && apt-get install -yq --no-install-recommends \
      libboost-dev libasound2-dev libx11-dev libxext-dev libxrandr-dev libxinerama-dev \
      libxcursor-dev libxcomposite-dev libfreetype-dev libfontconfig1-dev libcurl4-openssl-dev \
      libgl-dev ladspa-sdk ca-certificates curl \
 && rm -rf /var/lib/apt/lists/*

# Upstream's CMake FetchContents JUCE and sol2 at configure time; cache both (pinned by sha256) so
# build.sh runs offline via FETCHCONTENT_SOURCE_DIR_* + FETCHCONTENT_FULLY_DISCONNECTED (SPEC §6.2 item 19).
ARG JUCE_VER=8.0.13
ARG JUCE_SHA256=6aedfcaf2474f6a1d841081f12a21e03035e4c39da5805d0c2b51d05da1373b7
ARG SOL2_REV=c1f95a773c6f8f4fde8ca3efe872e7286afe4444
ARG SOL2_SHA256=f3b7bff03c260c74c74bd55f812f7e174d5c357f576430ffaf12a206462d6356
RUN d=/opt/toolchains/deps/juce-${JUCE_VER} && mkdir -p "$d/src" \
 && curl -fsSL -o /tmp/juce.tgz https://github.com/juce-framework/JUCE/archive/refs/tags/${JUCE_VER}.tar.gz \
 && echo "${JUCE_SHA256}  /tmp/juce.tgz" | sha256sum -c - \
 && tar -xzf /tmp/juce.tgz --strip-components=1 -C "$d/src" && rm /tmp/juce.tgz && chmod -R a+rX "$d" \
 && s=/opt/toolchains/deps/sol2-${SOL2_REV} && mkdir -p "$s/src" \
 && curl -fsSL -o /tmp/sol2.tgz https://github.com/ThePhD/sol2/archive/${SOL2_REV}.tar.gz \
 && echo "${SOL2_SHA256}  /tmp/sol2.tgz" | sha256sum -c - \
 && tar -xzf /tmp/sol2.tgz --strip-components=1 -C "$s/src" && rm /tmp/sol2.tgz && chmod -R a+rX "$s"
ENV ELEMENT_JUCE_SRC=/opt/toolchains/deps/juce-8.0.13/src \
    ELEMENT_SOL2_SRC=/opt/toolchains/deps/sol2-c1f95a773c6f8f4fde8ca3efe872e7286afe4444/src

USER mayhem
WORKDIR /mayhem
COPY --chown=2000:2000 . /mayhem

RUN if [ -x /opt/mayhem/ccache/prefill ]; then /opt/mayhem/ccache/prefill bash mayhem/build.sh; \
    else bash mayhem/build.sh; fi
RUN if [ -f mayhem/test.sh ]; then bash mayhem/test.sh; else echo "no mayhem/test.sh"; fi

LABEL org.opencontainers.image.source="${REPO_URL}" \
      org.opencontainers.image.revision="${COMMIT}" \
      rlenv.tier="commit" \
      rlenv.source_date_epoch="${SOURCE_DATE_EPOCH}" \
      rlenv.user="mayhem"

USER mayhem
WORKDIR /mayhem
