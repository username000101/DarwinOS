# ------------------------------------------------------------------------
#  RAVYNOS BUILD SYSTEM - Kernel and Extensions
# ------------------------------------------------------------------------

ROOT_SOURCE_DIR ?= ${.CURDIR}/..
_ROOT_BINARY_DIR ?= ${ROOT_SOURCE_DIR}/../build
ROOT_BINARY_DIR ?= ${_ROOT_BINARY_DIR:tA}
RAVYN_SDKROOT ?= ${ROOT_BINARY_DIR}/Developer/Platforms/ravynOS.platform/Developer/SDKs/ravynOS.sdk
SDK_SOURCE_DIR ?= ${ROOT_SOURCE_DIR}/Developer/ravynOS.sdk
SYSROOT_DIR ?= ${ROOT_BINARY_DIR}/sysroot
TOOLCHAIN ?= ${ROOT_BINARY_DIR}/Developer/Platforms/ravynOS.platform/Developer/Toolchains/Default.xctoolchain
TOOLS ?= ${TOOLCHAIN}/usr/bin
MACOS_VERSION_MIN ?= 10.15

.export ROOT_SOURCE_DIR ROOT_BINARY_DIR RAVYN_SDKROOT SDK_SOURCE_DIR SYSROOT_DIR TOOLCHAIN TOOLS MACOS_VERSION_MIN ARCH_CONFIGS KERNEL_CONFIGS

.if empty(ARCH_CONFIGS)
ARCH_CONFIGS = X86_64
.endif
.if empty(KERNEL_CONFIGS)
KERNEL_CONFIGS = RELEASE
.endif
.if empty(HOST_CC)
HOST_CC = /usr/bin/clang
.endif
.if empty(HOST_CXX)
HOST_CXX = /usr/bin/clang++
.endif
.if empty(HOST_LD)
HOST_LD = /usr/bin/ld
.endif
.if empty(CC)
CC = ${TOOLCHAIN}/usr/bin/clang
.endif
.if empty(CXX)
CXX = ${TOOLCHAIN}/usr/bin/clang++
.endif
.if empty(LD)
LD = ${TOOLCHAIN}/usr/bin/ld64
.endif
.if empty(AR)
AR = ${TOOLCHAIN}/usr/bin/llvm-ar --format=darwin
.endif

AVAILABILITY_PL = ${RAVYN_SDKROOT}/usr/local/libexec/availability.pl
AVAILABILITY_PL_PATH = ${RAVYN_SDKROOT}/usr/local/libexec/availability.pl
SEED_DEFINES = -DRC_ENABLE_XNU_PRODUCT_INFO_FILTER
PATH := ${TOOLCHAIN}/usr/bin:${PATH}

.if "${KERNEL_CONFIGS}" == "DEBUG"
DEBUG = .debug
.else
DEBUG =
.endif

.if "${.MAKE.OS}" ==  "Linux"
HOST_CODESIGN = /bin/true
IIG = /bin/true
HOST_CC = /usr/bin/clang
HOST_CXX = /usr/bin/clang++
HOST_LD = /usr/bin/ld
DKPATH = ${.OBJDIR}/xnu/${KERNEL_CONFIGS}_${ARCH_CONFIGS}/iokit/DriverKit

${DKPATH}: .NOTMAIN
	mkdir -p ${DKPATH}
	cp -Rf ${.CURDIR}/xnu/iokit/DriverKit/*.h ${DKPATH}/
TARGETS = ${DKPATH}
.endif
TARGETS += xnu_all

MAKEFLAGS=
.MAKEFLAGS=

PLATFORM_DIR=${ROOT_BINARY_DIR}/Developer/Platforms/ravynOS.platform

all: ${TARGETS}
installhdrs: .PHONY
	mkdir -p ${ROOT_BINARY_DIR}${.CURDIR}/xnu ${.OBJDIR}/xnu
	touch ${ROOT_BINARY_DIR}${.CURDIR}/xnu/.mach_kernel.timestamp
	touch ${ROOT_BINARY_DIR}${.CURDIR}/xnu/.symbolset.timestamp
	PATH="${PATH}" ${GMAKE} -C ${.CURDIR}/xnu LOGCOLORS=y PLATFORM=MacOSX BUILD_LTO=0 \
		BUILD_PROFILE=0 BUILD_WERROR=0 DO_CTFMERGE=0 \
		SDKVERSION=${MACOS_VERSION_MIN} SRCROOT=${.CURDIR}/xnu \
		OBJROOT=${.OBJDIR}/xnu DSTROOT=${RAVYN_SDKROOT} \
		MIGCOM=${MIGCOM} MIG=${MIG} IIG=${IIG} MIGCC=${CC} \
		CC=${CC} CXX=${CXX} HOST_OS_VERSION=${MACOS_VERSION_MIN} \
		UNIFDEF=${UNIFDEF} CTFCONVERT=${CTFCONVERT} \
		CTFMERGE=${CTFMERGE} CTFINSERT=${CTFINSERT} \
		HOST_SDKROOT=${RAVYN_SDKROOT} VERBOSE=yes \
		AVAILABILITY_PL=${AVAILABILITY_PL} SDKROOT=macosx \
		AVAILABILITY_PL_PATH=${AVAILABILITY_PL_PATH} XCRUN=${XCRUN} \
		KERNEL_CONFIGS=${KERNEL_CONFIGS} ARCH_CONFIGS=${ARCH_CONFIGS} \
		DEVELOPER_DIR=${ROOT_BINARY_DIR}/Developer HOST_FLEX=flex \
		SYMROOT=${ROOT_BINARY_DIR} HOST_CC=${HOST_CC} HOST_BISON=bison \
		HOST_CXX=${HOST_CXX} HOST_LD=${HOST_LD} TOOLCHAIN=${TOOLCHAIN} \
		HOST_CODESIGN=/bin/true HOST_CODESIGN_ALLOCATE=/bin/true installhdrs

libfirehose_kernel: .PHONY
	${MAKE} -C ${ROOT_SOURCE_DIR}/Libraries/libfirehose_kernel \
		ROOT_SOURCE_DIR="${ROOT_SOURCE_DIR}" \
		ROOT_BINARY_DIR="${ROOT_BINARY_DIR}" \
		RAVYN_SDKROOT="${RAVYN_SDKROOT}" \
		SDK_SOURCE_DIR="${SDK_SOURCE_DIR}" \
		SYSROOT_DIR="${SYSROOT_DIR}" \
		TOOLCHAIN="${TOOLCHAIN}" \
		TOOLS="${TOOLS}" \
		MACOS_VERSION_MIN="${MACOS_VERSION_MIN}" \
		CC="${CC}" CXX="${CXX}" LD="${LD}" AR="${AR}" all

compiler_rt: .PHONY
	${MAKE} -C ${ROOT_SOURCE_DIR}/Libraries \
		ROOT_SOURCE_DIR="${ROOT_SOURCE_DIR}" \
		ROOT_BINARY_DIR="${ROOT_BINARY_DIR}" \
		RAVYN_SDKROOT="${RAVYN_SDKROOT}" \
		SDK_SOURCE_DIR="${SDK_SOURCE_DIR}" \
		SYSROOT_DIR="${SYSROOT_DIR}" \
		TOOLCHAIN="${TOOLCHAIN}" \
		TOOLS="${TOOLS}" \
		DEVEL="${ROOT_SOURCE_DIR}/Developer" \
		BuildArch="X86" \
		CpuArch="x86_64" \
		MACOS_VERSION_MIN="${MACOS_VERSION_MIN}" \
		CC="${CC}" CXX="${CXX}" LD="${LD}" AR="${AR}" runtime

xnu_all: installhdrs libfirehose_kernel compiler_rt
	PATH="${PATH}" ${GMAKE} -C ${.CURDIR}/xnu LOGCOLORS=y \
		PLATFORM=MacOSX BUILD_LTO=0 BUILD_WERROR=0 DO_CTFMERGE=0 \
		SDKVERSION="${MACOS_VERSION_MIN}" SRCROOT="${.CURDIR}/xnu" \
		OBJROOT="${.OBJDIR}/xnu" DSTROOT="${RAVYN_SDKROOT}" \
		SYMROOT="${.OBJDIR}/xnu/sym" BUILD_STATIC_LINK=1 \
		MIGCOM="${MIGCOM}" MIG="${MIG}" IIG="${IIG}" CC="${CC}" CXX="${CXX}" \
		UNIFDEF="${UNIFDEF}" CTFCONVERT="${CTFCONVERT}" LD="${CC} --ld-path=${LD}" \
		CTFMERGE="${CTFMERGE}" CTFINSERT="${CTFINSERT}" AR="${AR}" \
		NM="${NM}" NMEDIT="${NMEDIT}" DSYMUTIL="${DSYMUTIL}" LIPO="${LIPO}" \
	    STRIP="${STRIPBIN}" AS="${AS}" AVAILABILITY_PL="${AVAILABILITY_PL}" \
		MACHINE="${MACHINE}" XCRUN="${XCRUN}" SDKROOT=macosx \
		HOST_OS_VERSION="${MACOS_VERSION_MIN}" VERBOSE=yes \
		AVAILABILITY_PL_PATH="${AVAILABILITY_PL_PATH}" \
		DEVELOPER_DIR="${ROOT_BINARY_DIR}/Developer" \
		LIBTOOL="${TOOLCHAIN}/usr/bin/libtool" VERBOSE=yes \
		KERNEL_CONFIGS="${KERNEL_CONFIGS}" ARCH_CONFIGS="${ARCH_CONFIGS}" \
		HOST_CC="${HOST_CC}" HOST_CXX="${HOST_CXX}" HOST_LD="${HOST_LD}" \
		TOOLCHAIN="${TOOLCHAIN}" HOST_CODESIGN=/bin/true \
		HOST_CODESIGN_ALLOCATE=/bin/true all install
.for target_dir in ${RAVYN_SDKROOT}/System/Library/Kernels \
  ${SYSROOT_DIR}/System/Library/Kernels
	mkdir -p ${target_dir} ${target_dir:H}/Extensions
	cp -Rf ${.OBJDIR}/xnu/sym/kernel${DEBUG}.dSYM ${target_dir}/
	cp -f ${.OBJDIR}/xnu/sym/kernel${DEBUG} ${target_dir}/
	cp -Rf ${.OBJDIR}/xnu/sym/System.kext ${target_dir:H}/Extensions/
.endfor

# NOTE! This kxld build is for the host toolchain. The target kxld is built
# later in IOKitUser
SUBDIR = xnu/libkern/kmod xnu/libkern/kxld
SUBDIR += IOKitUser
SUBDIR += Extensions

.include <bsd.subdir.mk>
