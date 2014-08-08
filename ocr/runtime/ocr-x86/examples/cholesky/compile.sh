for ocrbuild in .tbb "" ; do
    for arch in .x86 .phi; do
        if [[ ".x86" == "$arch" ]]; then
            compile_arch=""
        else
            compile_arch=$arch
        fi
        for model in .ocr .serialized; do
            for allocator in "" .tbb; do
                for dimension in "" .1D .1D.noalloc; do
                    for repeat in "" .repeat; do
                        if [[ ".ocr" == "$model" ]]; then 
                            for link in "" .static; do
                                OCR_INSTALL=$HOME/ocr.builds/ocr$arch.icc.env$ocrbuild LD_LIBRARY_PATH=$OCR_INSTALL/lib:$LD_LIBRARY_PATH CC=icc make compile$compile_arch$model$allocator$link$dimension$repeat.
                            done
                        else
                            CC=icc make compile$compile_arch$model$allocator$dimension$repeat.
                        fi
                    done
                done
            done
        done
    done

    if [[ ".tbb" == "$ocrbuild" ]]; then 
        for execs in `ls cholesky.*.ocr*`; do
            NEWNAME=`echo $execs | sed -e 's/ocr/tbb_linked_ocr/g'`
            mv $execs $NEWNAME
        done
    fi
done
