FROM fedora:37

RUN dnf update -y \
    && dnf install -y \
         curl \
         dbus-daemon \
         diffutils \
         gcc \
         gcovr \
         gettext \
         gi-docgen \
         git \
         gnupg2 \
         gtk4-devel \
         lcov \
         libasan \
         libgcrypt-devel \
         libsecret-devel \
         libubsan \
         meson \
         ninja-build \
         openssh \
         openssh-clients \
         p11-kit-devel \
         python \
         redhat-rpm-config \
         systemd-devel \
         unzip \
         vala \
    && dnf clean all

ARG HOST_USER_ID=5555
ENV HOST_USER_ID ${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -ms /bin/bash user

USER user
WORKDIR /home/user

ENV LANG C.UTF-8
