for ocrbuild in .tbb "" ; do
    for arch in .x86 .phi; do
        if [[ ".x86" == "$arch" ]]; then
            compile_arch=""
        else
            compile_arch=$arch
        fi
        for model in .ocr .serialized; do
            for allocator in "" .tbb; do
                for repeat in "" .repeat; do
                    for dimension in "" .1D .1D.noalloc; do
                        for link in "" .static; do
                            TARGETNAME=compile$compile_arch$model$allocator$link$dimension$repeat.
                            if [[ ".ocr" == "$model" ]]; then 
                                OCR_INSTALL=$HOME/ocr.builds/ocr$arch.icc.env$ocrbuild LD_LIBRARY_PATH=$OCR_INSTALL/lib:$LD_LIBRARY_PATH CC=icc make -f Makefile.generate $TARGETNAME
                            else
                                CC=icc make -f Makefile.generate $TARGETNAME
                            fi
                            done
                        if [[ ".1D" == "$dimension" ]]; then 
                            for mklmodel in .seq .multi; do
                                for link in "" .static; do
                                    if [[ ".ocr" == "$model" ]]; then 
                                        OCR_INSTALL=$HOME/ocr.builds/ocr$arch.icc.env$ocrbuild LD_LIBRARY_PATH=$OCR_INSTALL/lib:$LD_LIBRARY_PATH CC=icc make -f Makefile.generate compile$compile_arch$model$allocator$link$mklmodel.mkl$dimension$repeat.
                                    else
                                        for kernel in "" .dpotrf; do
                                            CC=icc make -f Makefile.generate compile$compile_arch$model$allocator$link$mklmodel.mkl$dimension$repeat$kernel.
                                        done
                                    fi
                                done
                            done
                        fi
                    done
                done
            done
        done
    done

    if [[ ".tbb" == "$ocrbuild" ]]; then 
        for execs in `ls cholesky.*.ocr*xec`; do
            NEWNAME=`echo $execs | sed -e 's/ocr/tbb_linked_ocr/g'`
            mv $execs $NEWNAME
        done
    fi
done
