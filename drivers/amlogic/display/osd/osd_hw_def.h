#ifndef   _OSD_HW_DEF_H
#define	_OSD_HW_DEF_H
#include <linux/osd/osd_hw.h>
#include <linux/amports/vframe_provider.h>
#include <linux/fiq_bridge.h>

/************************************************************************
**
**	macro  define  part
**
**************************************************************************/
#define	LEFT		0
#define	RIGHT		1
#define  	OSD_RELATIVE_BITS				0x333f0
#define    HW_OSD_COUNT					2

/************************************************************************
**
**	typedef  define  part
**
**************************************************************************/
typedef  void (*update_func_t)(void) ;   

typedef  struct{
	struct list_head  	list ;
	update_func_t    	update_func;  //each reg group has it's own update function.
}hw_list_t;

typedef  struct{
	u32  width;  //in byte unit
	u32	height; 
	u32  canvas_idx;
	u32	addr;
}fb_geometry_t;
typedef  struct{
	u16	h_enable;
	u16	v_enable;
}osd_scale_t;
typedef  struct{
	osd_scale_t  origin_scale;
	u16  enable;
	u16  left_right;
	u16  l_start;
	u16  l_end;
	u16  r_start;
	u16  r_end;
}osd_3d_mode_t; 
typedef  pandata_t  dispdata_t;

typedef  struct {
	pandata_t 		pandata[HW_OSD_COUNT];
	dispdata_t		dispdata[HW_OSD_COUNT];
	u32  			gbl_alpha[HW_OSD_COUNT];
	u32  			color_key[HW_OSD_COUNT];
	u32				color_key_enable[HW_OSD_COUNT];
	u32				enable[HW_OSD_COUNT];
	u32				reg_status_save;
	bridge_item_t 		fiq_handle_item;
	osd_scale_t		scale[HW_OSD_COUNT];
	u32				free_scale_enable[HW_OSD_COUNT];
	u32				free_scale_width[HW_OSD_COUNT];
	u32				free_scale_height[HW_OSD_COUNT];
	fb_geometry_t		fb_gem[HW_OSD_COUNT];
	const color_bit_define_t *color_info[HW_OSD_COUNT];
	u32				scan_mode;
	u32				osd_order;
	osd_3d_mode_t	mode_3d[HW_OSD_COUNT];
	u32			updated[HW_OSD_COUNT];	
	hw_list_t	 	reg[HW_OSD_COUNT][HW_REG_INDEX_MAX];
}hw_para_t;

/************************************************************************
**
**	func declare  part
**
**************************************************************************/

static  void  osd2_update_color_mode(void);
static  void  osd2_update_enable(void);
static  void  osd2_update_color_key_enable(void);
static  void  osd2_update_color_key(void);
static  void  osd2_update_gbl_alpha(void);
static  void  osd2_update_order(void);
static  void  osd2_update_disp_geometry(void);
static  void  osd2_update_disp_scale_enable(void);
static  void  osd2_update_disp_3d_mode(void);

static  void  osd1_update_color_mode(void);
static  void  osd1_update_enable(void);
static  void  osd1_update_color_key(void);
static  void  osd1_update_color_key_enable(void);
static  void  osd1_update_gbl_alpha(void);
static  void  osd1_update_order(void);
static  void  osd1_update_disp_geometry(void);
static  void  osd1_update_disp_scale_enable(void);
static  void  osd1_update_disp_3d_mode(void);


/************************************************************************
**
**	global varible  define  part
**
**************************************************************************/
LIST_HEAD(update_list);
static spinlock_t osd_lock = SPIN_LOCK_UNLOCKED;
static hw_para_t  osd_hw;
static unsigned long 	lock_flags;
#ifdef FIQ_VSYNC
static unsigned long	fiq_flag;
#endif
static vframe_t vf,vf_w;
static update_func_t     hw_func_array[HW_OSD_COUNT][HW_REG_INDEX_MAX]={
	{
		osd1_update_color_mode,
		osd1_update_enable,
		osd1_update_color_key,
		osd1_update_color_key_enable,
		osd1_update_gbl_alpha,
		osd1_update_order,
		osd1_update_disp_geometry,
		osd1_update_disp_scale_enable,
	},
	{
		osd2_update_color_mode,
		osd2_update_enable,
		osd2_update_color_key,
		osd2_update_color_key_enable,
		osd2_update_gbl_alpha,
		osd2_update_order,
		osd2_update_disp_geometry,
		osd2_update_disp_scale_enable,
	},
};

#ifdef FIQ_VSYNC
#define add_to_update_list(osd_idx,cmd_idx) \
	spin_lock_irqsave(&osd_lock, lock_flags); \
	raw_local_save_flags(fiq_flag); \
	local_fiq_disable(); \
	osd_hw.updated[osd_idx]|=(1<<cmd_idx); \
	raw_local_irq_restore(fiq_flag); \
	spin_unlock_irqrestore(&osd_lock, lock_flags);
#else
#define add_to_update_list(osd_idx,cmd_idx) \
	spin_lock_irqsave(&osd_lock, lock_flags); \
	osd_hw.updated[osd_idx]|=(1<<cmd_idx); \
	spin_unlock_irqrestore(&osd_lock, lock_flags); 
#endif




#define remove_from_update_list(osd_idx,cmd_idx) \
	osd_hw.updated[osd_idx]&=~(1<<cmd_idx);
#endif
