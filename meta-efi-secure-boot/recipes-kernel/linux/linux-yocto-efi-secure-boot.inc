DEPENDS += "openssl-native"
FILESEXTRAPATHS_prepend := "${THISDIR}/linux-yocto:"

efi_secure_boot_sccs = "\
    ${@bb.utils.contains('DISTRO_FEATURES', 'efi-secure-boot', \
                         'cfg/efi-ext.scc', '', d)} \
"
KERNEL_FEATURES_append_x86 += "${efi_secure_boot_sccs}"
KERNEL_FEATURES_append_x86-64 += "${efi_secure_boot_sccs}"

inherit user-key-store

fakeroot python do_sign() {
    import re

    if (d.expand('${TARGET_ARCH}') != 'x86_64') and (not re.match('i.86', d.expand('${TARGET_ARCH}'))):
        return

    if d.expand('${UEFI_SB}') != '1':
        return

    import shutil

    for type in d.expand('${KERNEL_IMAGETYPES}').split():
        kernel = d.expand('${B}/${KERNEL_OUTPUT_DIR}/') + type

        # Prepare the unsigned kernel image for manual signing.
        shutil.copy(kernel, d.expand('${B}/') + type + '.unsigned')

        # SELoader signature is always based on the unsigned kernel image,
        # disallowing chainloader to kernel efi-stub.
        uks_sel_sign(kernel, d)

        shutil.copyfile(kernel, d.expand('${D}/boot/') + type + d.expand('-${KERNEL_RELEASE}'))
        shutil.copyfile(kernel + '.p7b', d.expand('${D}/boot/') + type + d.expand('-${KERNEL_RELEASE}.p7b'))
        os.symlink(type + d.expand('-${KERNEL_RELEASE}.p7b'), d.expand('${D}/boot/') + type + '.p7b')
}

# Make sure the kernel image has been signed before kernel_do_deploy()
# which prepares the kernel image for creating usb/iso.
addtask sign after do_install before do_package do_populate_sysroot do_deploy
do_sign[prefuncs] += "check_deploy_keys"

fakeroot python do_sign_bundled_kernel() {
    import re

    if (d.expand('${TARGET_ARCH}') != 'x86_64') and (not re.match('i.86', d.expand('${TARGET_ARCH}'))):
        return

    if d.expand('${UEFI_SB}') != '1':
        return

    if (d.expand('${INITRAMFS_IMAGE}') == '') or (d.expand('${INITRAMFS_IMAGE_BUNDLE}') != '1'):
        return

    import shutil

    for type in d.expand('${KERNEL_IMAGETYPES}').split():
        kernel = d.expand('${B}/${KERNEL_OUTPUT_DIR}/') + type + '.initramfs'

        # Prepare the unsigned kernel image for manual signing.
        shutil.copy(kernel, d.expand('${B}/') + type + '.initramfs.unsigned')

        # SELoader signature is always based on the unsigned kernel image,
        # disallowing chainloader to kernel efi-stub.
        uks_sel_sign(kernel, d)

        shutil.copyfile(kernel, d.expand('${D}/boot/') + type + d.expand('-initramfs-${MACHINE}.bin'))
        shutil.copyfile(kernel + '.p7b', d.expand('${D}/boot/') + type + d.expand('-initramfs-${MACHINE}.bin.p7b'))
}
addtask sign_bundled_kernel after do_bundle_initramfs before do_deploy

do_deploy_append() {
    install -d "${DEPLOYDIR}/efi-unsigned"

    for imageType in ${KERNEL_IMAGETYPES}; do
        if [ -f "${B}/$imageType.unsigned" ]; then
            install -m 0644 "${B}/$imageType.unsigned" "${DEPLOYDIR}/efi-unsigned/$imageType"
        fi

        if [ -f "${B}/$imageType.initramfs.unsigned" ]; then
            install -m 0644 "${B}/$imageType.initramfs.unsigned" "${DEPLOYDIR}/efi-unsigned/$imageType.initramfs"
        fi

        if [ -f "${D}/boot/$imageType-initramfs-${MACHINE}.bin.p7b" ]; then
            install -m 0644 "${D}/boot/$imageType-initramfs-${MACHINE}.bin.p7b" "${DEPLOYDIR}"
        fi

        if [ -f "${B}/${KERNEL_OUTPUT_DIR}/$imageType.p7b" ]; then
            base_name="${imageType}-${KERNEL_IMAGE_NAME}.bin.p7b"

            install -m 0644 "${B}/${KERNEL_OUTPUT_DIR}/$imageType.p7b" "${DEPLOYDIR}/$base_name"
            ln -sf "$base_name" "${DEPLOYDIR}/$imageType-${KERNEL_IMAGE_LINK_NAME}.bin.p7b"
            ln -sf "$base_name" "${DEPLOYDIR}/$imageType.p7b"
        fi
    done
}

# Ship *.p7b files to related packages
python do_package_prepend() {
    for type in d.expand('${KERNEL_IMAGETYPES}').split():
        typelower = type.lower()
        d.appendVar('FILES_kernel-image-' + typelower, ' /boot/' + type + d.expand('-${KERNEL_VERSION_NAME}.p7b'))
        d.appendVar('FILES_kernel-image-' + typelower, ' /boot/' + type + '.p7b')
}
