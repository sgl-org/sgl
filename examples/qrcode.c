/**
 * @file sgl/examples/qrcode.c
 * @brief QR Code widget example
 */

#include <sgl.h>

/**
 * @brief create the qrcode example
 * @param parent parent object, NULL creates the qrcode on the active screen
 * @return none
 */
void sgl_qrcode_examples(sgl_obj_t *parent)
{
    sgl_obj_t *qrcode;
    
    /* Create QR code widget */
    qrcode = sgl_qrcode_create(parent);
    if (qrcode == NULL) return;
    
    /* Set position and size - QR code version 5 is 37x37 modules, scale=8 gives ~296px */
    sgl_obj_set_pos(qrcode, 30, 40);
    sgl_obj_set_size(qrcode, 80, 80);

    /* Set ECC level [0-3], version [1-40] */
    sgl_qrcode_set_ecc(qrcode, 2);          /* Medium error correction */
    sgl_qrcode_set_version(qrcode, 5);      /* QR version 5 (37x37 modules) */
    
    /* Set the text/content to encode */
    sgl_qrcode_set_text(qrcode, "https://github.com/sgl-org/sgl");
    
    /* Set colors */
    sgl_qrcode_set_bg_color(qrcode, SGL_COLOR_WHITE);
    sgl_qrcode_set_cell_color(qrcode, SGL_COLOR_BLACK);
    sgl_qrcode_set_alpha(qrcode, 255);
    
    /* Optional: set margins and scaling */
    sgl_qrcode_set_zone(qrcode, 1);         /* Quiet zone/margin */
    sgl_qrcode_set_scale(qrcode, 2);        /* Module scale factor */
}
