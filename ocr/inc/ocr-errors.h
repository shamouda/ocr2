/**
 * @brief Error codes for OCR
 */

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __OCR_ERRORS_H__
#define __OCR_ERRORS_H__

/**
 * @ingroup OCRTypes
 * @{
 *
 * @defgroup OCRTypesErrors Errors returned by the OCR APIs
 * @{
 */
// OCR error codes (loosely based on linux error.h)

#define OCR_EPERM            1      /**< Operation not permitted */
#define OCR_ENOENT           2      /**< No such file or directory */
#define OCR_EINTR            4      /**< Interrupted OCR runtime call */
#define OCR_EIO              5      /**< I/O error */
#define OCR_ENXIO            6      /**< No such device or address */
#define OCR_E2BIG            7      /**< Argument list too long */
#define OCR_ENOEXEC          8      /**< Exec format error */
#define OCR_EAGAIN          11      /**< Try again */
#define OCR_ENOMEM          12      /**< Out of memory */
#define OCR_EACCES          13      /**< Permission denied */
#define OCR_EFAULT          14      /**< Bad address */
#define OCR_EBUSY           16      /**< Device or resource busy */
#define OCR_ENODEV          19      /**< No such device */
#define OCR_EINVAL          22      /**< Invalid argument */
#define OCR_ENOSPC          28      /**< No space left on device */
#define OCR_ESPIPE          29      /**< Illegal seek */
#define OCR_EROFS           30      /**< Read-only file system */
#define OCR_EDOM            33      /**< Math argument out of domain of func */
#define OCR_ERANGE          34      /**< Math result not representable */
#define OCR_ENOSYS          38      /**< Function not implemented */
#define OCR_ENOTEMPTY       39      /**< Directory not empty */
#define OCR_ENOTSUP         95      /**< Function is not supported */

// OCR specific errors
#define OCR_EGUIDEXISTS     100     /**< The object referred to by the given GUID already exists */
#define OCR_EACQ            101     /**< Data block is already acquired */
#define OCR_EPEND           102     /**< Operation is pending */
#define OCR_ENOP            103     /**< Operation was a no-op */
#define OCR_ECANCELED       125     /**< Operation canceled */
#define OCR_ACQ_DEST_DEAD       126     /**< Acquire DB at a dead location  */

/**
 * @}
 * @}
 */
#endif /* __OCR_ERRORS_H__ */
