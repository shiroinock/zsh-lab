FROM alpine:3.24@sha256:f59abe656b692d901e184be3445e084786602108599d2a51d229c0f5a48c71bf AS build
RUN apk add --no-cache build-base ncurses-dev zsh vim
WORKDIR /src
COPY VERSION .
COPY src ./src
RUN cc -O2 -Wall -Wextra -Wno-misleading-indentation -DZSHLAB_CONTAINER_BUILD -DZSHLAB_VERSION=\"$(cat VERSION)\" src/app.c -lncursesw -lutil -o /zsh-lab \
 && cc -O2 -Wno-unused-function -DZSHLAB_CONTAINER_BUILD src/tests.c -lncursesw -lutil -o /test-native

FROM alpine:3.24@sha256:f59abe656b692d901e184be3445e084786602108599d2a51d229c0f5a48c71bf AS runtime
RUN apk add --no-cache zsh vim ncurses-terminfo-base \
 && adduser -D -u 1000 learner \
 && mkdir -p /work /state && chown learner:learner /work /state
COPY --from=build /zsh-lab /usr/local/bin/zsh-lab
ENV HOME=/home/learner TERM=xterm-256color LANG=C.UTF-8 EDITOR="vim -Nu NONE -i NONE" VISUAL="vim -Nu NONE -i NONE"
USER 1000:1000
WORKDIR /work
ENTRYPOINT ["/usr/local/bin/zsh-lab"]

FROM runtime AS test
COPY --from=build /test-native /usr/local/libexec/test-native
