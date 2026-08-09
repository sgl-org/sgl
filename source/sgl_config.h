//********************************************************************
//* SGL Configuration File                                           *
//* You can modify the following parameters according to your needs. *
//********************************************************************

#ifndef  __SGL_CONFIG_H__
#define  __SGL_CONFIG_H__


#define  CONFIG_SGL_FBDEV_PIXEL_DEPTH                      16
#define  CONFIG_SGL_FBDEV_ROTATION                         0
#define  CONFIG_SGL_USE_FBDEV_VRAM                         0
#define  CONFIG_SGL_SYSTICK_MS                             10 
#define  CONFIG_SGL_EVENT_QUEUE_SIZE                       32
#define  CONFIG_SGL_DIRTY_AREA_TRACE                        0
#define  CONFIG_SGL_DIRTY_AREA_NUM_MAX                     128
#define  CONFIG_SGL_COLOR16_SWAP                           0   
#define  CONFIG_SGL_ANIMATION                              1  
#define  CONFIG_SGL_DEBUG                                  0 
#define  CONFIG_SGL_LOG_COLOR                              1  
#define  CONFIG_SGL_LOG_LEVEL                              0  
#define  CONFIG_SGL_OBJ_USE_NAME                           0  
#define  CONFIG_SGL_FONT_COMPRESSED                        1
#define  CONFIG_SGL_BOOT_LOGO                              1
#define  CONFIG_SGL_THEME_DARK                             0      
#define  CONFIG_SGL_HEAP_ALGO                              lwmem
#define  CONFIG_SGL_HEAP_MEMORY_SIZE                       102400
#define  CONFIG_SGL_TLSF_INDEX_MAX                         16
#define  CONFIG_SGL_FONT_SONG23                            1      
#define  CONFIG_SGL_FONT_CONSOLAS14                        1      
#define  CONFIG_SGL_FONT_CONSOLAS23                        1      
#define  CONFIG_SGL_FONT_CONSOLAS24                        1      
#define  CONFIG_SGL_FONT_CONSOLAS32                        1      
#define  CONFIG_SGL_FONT_CONSOLAS24_COMPRESS               1            
#define  CONFIG_SGL_MONITOR_TRACE                          1
#define CONFIG_SGL_PIXMAP_BILINEAR_INTERP                   1

#endif  //!__SGL_CONFIG_H__
