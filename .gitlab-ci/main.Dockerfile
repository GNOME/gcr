FROM fedora:41

RUN dnf update -y \
    && dnf install -y \
           curl \
           dbus-daemon \
           diffutils \
           gcc \
           gcovr \
           gi-docgen \
           git \
           gnutls-devel \
           gobject-introspection-devel \
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
           python \
           systemd-devel \
           unzip \
           vala \
           valgrind-devel \
    && dnf clean all

# Enable sudo for wheel users
RUN sed -i -e 's/# %wheel/%wheel/' -e '0,/%wheel/{s/%wheel/# %wheel/}' /etc/sudoers

ARG HOST_USER_ID=5555
ENV HOST_USER_ID ${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -G wheel -ms /bin/bash user

USER user
WORKDIR /home/user

ENV LANG C.UTF-8
