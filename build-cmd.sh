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

echo "${VERSION}.0" > "${stage}/VERSION.txt"

pushd "$SOURCE_DIR"
case "$AUTOBUILD_PLATFORM" in
    windows*|darwin*)
        # Given forking and future development work, it seems unwise to
        # hardcode the actual URL of the current project's libndofdev
        # repository in this message. Try to determine the URL of this
        # open-libndofdev repository and remove "open-" as a suggestion.
        echo "Windows/Mac libndofdev is in a separate GitHub repository" 1>&2 ; exit 1
    ;;
    linux*)
        # Linux build environment at Linden comes pre-polluted with stuff that can
        # seriously damage 3rd-party builds.  Environmental garbage you can expect
        # includes:
        #
        #    DISTCC_POTENTIAL_HOSTS     arch           root        CXXFLAGS
        #    DISTCC_LOCATION            top            branch      CC
        #    DISTCC_HOSTS               build_name     suffix      CXX
        #    LSDISTCC_ARGS              repo           prefix      CFLAGS
        #    cxx_version                AUTOBUILD      SIGN        CPPFLAGS
        #
        # So, clear out bits that shouldn't affect our configure-directed build
        # but which do nonetheless.
        #
        unset DISTCC_HOSTS CFLAGS CPPFLAGS CXXFLAGS

        # Default target per --address-size
        opts_c="${TARGET_OPTS:--m$AUTOBUILD_ADDRSIZE $LL_BUILD_RELEASE_CFLAGS}"
        opts_cxx="${TARGET_OPTS:--m$AUTOBUILD_ADDRSIZE $LL_BUILD_RELEASE_CXXFLAGS}"

        # release build
        CFLAGS="$opts_c -I${stage}/packages/include -Wl,-L${stage}/packages/lib/release" \
        CXXFLAGS="$opts_cxx -I${stage}/packages/include -Wl,-L${stage}/packages/lib/release" \
        LDFLAGS="-L${stage}/packages/lib/release" \
        USE_SDL2=1 \
        make all

        cp libndofdev.a ${stage_release}
        cp ndofdev_external.h ${stage_include}
    ;;
esac

mkdir -p ${stage}/LICENSES
cp LICENSE ${stage}/LICENSES/libndofdev.txt

popd
