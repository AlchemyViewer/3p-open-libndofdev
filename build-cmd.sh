#!/usr/bin/env bash

cd "$(dirname "$0")"

# turn on verbose debugging output for parabuild logs.
exec 4>&1; export BASH_XTRACEFD=4; set -x
# make errors fatal
set -e
# complain about unset env variables
set -u

if [ -z "$AUTOBUILD" ] ; then
    exit 1
fi

if [ "$OSTYPE" = "cygwin" ] ; then
    autobuild="$(cygpath -u $AUTOBUILD)"
else
    autobuild="$AUTOBUILD"
fi


top="$(pwd)"
stage="$(pwd)/stage"
stage_include="$stage/include/"
stage_debug="$stage/lib/debug/"
stage_release="$stage/lib/release/"

mkdir -p ${stage_include}
mkdir -p ${stage_debug}
mkdir -p ${stage_release}

PROJECT="libndofdev"
# 2nd line of CHANGELOG is most recent version number:
#         * 0.3
# Tease out just the version number from that line.
VERSION="$(expr "$(sed -n 2p "$PROJECT/CHANGELOG")" : ".* \([0-9]*\.[0-9]*\) *$")"
SOURCE_DIR="$PROJECT"

"$autobuild" source_environment > "$stage/variables_setup.sh" || exit 1
. "$stage/variables_setup.sh"


build=${AUTOBUILD_BUILD_ID:=0}
echo "${VERSION}.${build}" > "${stage}/VERSION.txt"

pushd "$SOURCE_DIR"
case "$AUTOBUILD_PLATFORM" in
    windows*|darwin*)
        # Given forking and future development work, it seems unwise to
        # hardcode the actual URL of the current project's libndofdev
        # repository in this message. Try to determine the URL of this
        # open-libndofdev repository and remove "open-" as a suggestion.
        echo "Windows/Mac libndofdev is in a separate bitbucket repository \
        -- try $(hg paths default | sed -E 's/open-(libndofdev)/\1/')" 1>&2 ; exit 1
    ;;
    linux*)
        opts="${TARGET_OPTS:--m$AUTOBUILD_ADDRSIZE}"
        DEBUG_COMMON_FLAGS="$opts -Og -g -fPIC -DPIC"
        RELEASE_COMMON_FLAGS="$opts -O3 -g -fPIC -fstack-protector-strong -DPIC -D_FORTIFY_SOURCE=2"
        DEBUG_CFLAGS="$DEBUG_COMMON_FLAGS"
        RELEASE_CFLAGS="$RELEASE_COMMON_FLAGS"
        DEBUG_CXXFLAGS="$DEBUG_COMMON_FLAGS -std=c++17"
        RELEASE_CXXFLAGS="$RELEASE_COMMON_FLAGS -std=c++17"
        DEBUG_CPPFLAGS="-DPIC"
        RELEASE_CPPFLAGS="-DPIC -D_FORTIFY_SOURCE=2"
        
        JOBS=`cat /proc/cpuinfo | grep processor | wc -l`
        
        # debug build
        CFLAGS="$DEBUG_CFLAGS -I${stage}/packages/include -Wl,-L${stage}/packages/lib/debug" \
        CXXFLAGS="$DEBUG_CXXFLAGS -I${stage}/packages/include -Wl,-L${stage}/packages/lib/debug" \
        CPPFLAGS="$DEBUG_CPPFLAGS -I${stage}/packages/include -Wl,-L${stage}/packages/lib/debug" \
        LDFLAGS="$opts -L${stage}/packages/lib/debug" \
        make all

        cp libndofdev.a ${stage_debug}

        make clean

        # release build
        CFLAGS="$RELEASE_CFLAGS -I${stage}/packages/include -Wl,-L${stage}/packages/lib/release" \
        CXXFLAGS="$RELEASE_CXXFLAGS -I${stage}/packages/include -Wl,-L${stage}/packages/lib/release" \
        CPPFLAGS="$RELEASE_CPPFLAGS -I${stage}/packages/include -Wl,-L${stage}/packages/lib/release" \
        LDFLAGS="$opts -L${stage}/packages/lib/release" \
        make all

        cp libndofdev.a ${stage_release}

        make clean

        cp ndofdev_external.h ${stage_include}
    ;;
esac
popd

mkdir -p ${stage}/LICENSES
cp LICENSE ${stage}/LICENSES/libndofdev.txt