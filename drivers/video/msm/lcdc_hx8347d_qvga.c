
#include <linux/delay.h>
#include <mach/gpio.h>
#include "msm_fb.h"
#include "lcdc_huawei_config.h"

#define TRACE_LCD_DEBUG 0
#if TRACE_LCD_DEBUG
#define LCD_DEBUG(x...) printk(KERN_ERR "[LCD_DEBUG] " x)
#else
#define LCD_DEBUG(x...) do {} while (0)
#endif

#define lCD_DRIVER_NAME "lcdc_hx8347d_qvga"

#define LCD_MIN_BACKLIGHT_LEVEL 0
#define LCD_MAX_BACKLIGHT_LEVEL	255

#define DEVICE_ID 				0x70 //BS0=1
#define WRITE_REGISTER 			0x00
#define WRITE_CONTENT 			0x02


struct hx8347d_state_type{
	boolean disp_initialized;
	boolean display_on;
	boolean disp_powered_up;
};

extern void pwm_set_backlight(int level);

static int lcd_reset_gpio;
static struct hx8347d_state_type hx8347d_state = { 0 };
static struct msm_panel_common_pdata *lcdc_hx8347d_pdata;
static lcd_panel_type lcd_panel_qvga = LCD_NONE;

struct command{
	uint32 reg;
	uint32 value;
	uint32 time;
};

/* del the initial code here, initialize it in arm9 */
static const struct command hx8347d_truly_disp_off[] = 
{
    {0x28, 0x38, 40},
    {0x1F, 0x89, 40},
    {0x28, 0x04, 40},
    {0x28, 0x24, 40},
    {0x19, 0x00, 5},
};

static const struct command hx8347d_truly_disp_on[] = 
{
    { 0x18, 0x36, 0},
    { 0x19, 0x01, 0},
    { 0x1F, 0x88, 5},
    { 0x1F, 0x80, 5},
    { 0x1F, 0x90, 5},
    { 0x1F, 0xD0, 5},
    { 0x28, 0x38, 40},
    { 0x28, 0x3C, 0},
};

static const struct command hx8347d_innolux_disp_off[] = 
{
    {0x28, 0x38, 40},
    {0x1F, 0x89, 40},
    {0x28, 0x04, 40},
    {0x28, 0x24, 40},
    {0x19, 0x00, 5},
};
static const struct command hx8347d_innolux_disp_on[] = 
{
    { 0x18, 0x66, 0},
    { 0x19, 0x01, 0},
    { 0x1F, 0x88, 5},
    { 0x1F, 0x80, 5},
    { 0x1F, 0x90, 5},
    { 0x1F, 0xD4, 5},
    { 0x28, 0x38, 40},
    { 0x28, 0x3C, 120},
};

static const struct command hx8347d_display_area_table[] = 
{
	/* Select command page */
	{ 0xFF, 0x00, 0},
		
	/*Column address start register*/
	{ 0x02, 0x00, 0},
	{ 0x03, 0x00, 0},
	
	/*Column address end register*/
	{ 0x04, 0x01, 0},
	{ 0x05, 0xDF, 0},
	
	/*Row address start register*/
	{ 0x06, 0x00, 0},
	{ 0x07, 0x00, 0},
	
	/*Row address end register*/
	{ 0x08, 0x01, 0},
	{ 0x09, 0x3F, 0},
};

static void _serigo(uint8 reg, uint8 data)
{
    uint8 start_byte_reg = DEVICE_ID | WRITE_REGISTER;
    uint8 start_byte_data = DEVICE_ID | WRITE_CONTENT;
    
    seriout_transfer_byte(reg, start_byte_reg);
    seriout_transfer_byte(data, start_byte_data);
}

static void process_lcdc_table(struct command *table, size_t count)
{
    int i;
    uint32 reg = 0;
    uint32 value = 0;
    uint32 time = 0;

    for (i = 0; i < count; i++) {
        reg = table[i].reg;
        value = table[i].value;
        time = table[i].time;

        _serigo(reg, value);

        if (time != 0)
        	mdelay(time);
    }
}

static void hx8347d_disp_powerup(void)
{
    if (!hx8347d_state.disp_powered_up && !hx8347d_state.display_on) {
        /* Reset the hardware first */
        /* Include DAC power up implementation here */
        hx8347d_state.disp_powered_up = TRUE;
    }
}

static void hx8347d_disp_on(void)
{
    if (hx8347d_state.disp_powered_up && !hx8347d_state.display_on) 
    {
        LCD_DEBUG("%s: disp on lcd\n", __func__);
		/* del comment lines */
        hx8347d_state.display_on = TRUE;
    }
}

static void hx8347d_reset(void)
{
    /* Reset LCD*/
    lcdc_hx8347d_pdata->panel_config_gpio(1);
    lcd_reset_gpio = *(lcdc_hx8347d_pdata->gpio_num + 4);
    
}
void lcdc_hx8347d_cabc_set_backlight(uint8 level)
{  
    /* Select Command Page0 */
    _serigo(0xFF, 0x00);
    
    /*BCTRL	=1	(Backlight Control Block, This bit is always used to switch brightness for display.)
       DD	=1	(Display Dimming)
       BL		=1	(Backlight Control)*/ 
    _serigo(0x3D, 0x2C);
    /*Set backlight level*/
    _serigo(0x3C, level);

    /* Set User Interface Image mode for content adaptive image functionality*/
    _serigo(0x3E, 0x01);

    /* set the minimum brightness value of the display for CABC function*/
    _serigo(0x3F, 0x30);

    _serigo(0xFF, 0x01);
    _serigo(0xC3, 0x0F);
    _serigo(0xC5, 0x27); 
    
    _serigo(0xCB, 0x24);
    _serigo(0xCC, 0x24);
    _serigo(0xCD, 0x24);
    _serigo(0xCE, 0x23);
    _serigo(0xCF, 0x23);
    _serigo(0xD0, 0x23); 
    _serigo(0xD1, 0x22);
    _serigo(0xD2, 0x22);
    _serigo(0xD3, 0x22);
    
    _serigo(0xFF, 0x00);
	
}

static int hx8347d_panel_on(struct platform_device *pdev)
{
    if (!hx8347d_state.disp_initialized) 
    {
        hx8347d_reset();
        lcd_spi_init(lcdc_hx8347d_pdata);	/* LCD needs SPI */
        hx8347d_disp_powerup();
        hx8347d_disp_on();
        hx8347d_state.disp_initialized = TRUE;
        LCD_DEBUG("%s: hx8347d lcd initialized\n", __func__);
    } 
    else if (!hx8347d_state.display_on) 
    {
        /* Exit Standby Mode */
		switch (lcd_panel_qvga)
		{
			case LCD_HX8347D_TRULY_QVGA:
		        process_lcdc_table((struct command*)&hx8347d_truly_disp_on, ARRAY_SIZE(hx8347d_truly_disp_on));
				break;
				
			case LCD_HX8347D_INNOLUX_QVGA:
		        process_lcdc_table((struct command*)&hx8347d_innolux_disp_on, ARRAY_SIZE(hx8347d_innolux_disp_on));
				break;
				
			default:
		        process_lcdc_table((struct command*)&hx8347d_truly_disp_on, ARRAY_SIZE(hx8347d_truly_disp_on));
				break;
		}

		lcdc_hx8347d_cabc_set_backlight(0xFF);
		
        LCD_DEBUG("%s: Exit Standby Mode\n", __func__);
        hx8347d_state.display_on = TRUE;
    }
    
    return 0;
}

static int hx8347d_panel_off(struct platform_device *pdev)
{
    if (hx8347d_state.disp_powered_up && hx8347d_state.display_on) {
        /* Enter Standby Mode */
		switch (lcd_panel_qvga)
		{
			case LCD_HX8347D_TRULY_QVGA:
		        process_lcdc_table((struct command*)&hx8347d_truly_disp_off, ARRAY_SIZE(hx8347d_truly_disp_off));
				break;
				
			case LCD_HX8347D_INNOLUX_QVGA:
		        process_lcdc_table((struct command*)&hx8347d_innolux_disp_off, ARRAY_SIZE(hx8347d_innolux_disp_off));
				break;
				
			default:
		        process_lcdc_table((struct command*)&hx8347d_truly_disp_off, ARRAY_SIZE(hx8347d_truly_disp_off));
				break;
		}
		
        hx8347d_state.display_on = FALSE;
        LCD_DEBUG("%s: Enter Standby Mode\n", __func__);
    }

    return 0;
}

static void hx8347d_set_backlight(struct msm_fb_data_type *mfd)
{
    int bl_level = mfd->bl_level;
       
   // lcd_set_backlight_pwm(bl_level);
    pwm_set_backlight(bl_level);
     return;
}

static void hx8347d_panel_set_contrast(struct msm_fb_data_type *mfd, unsigned int contrast)
{

    return;
}

static int __init hx8347d_probe(struct platform_device *pdev)
{
    if (pdev->id == 0) {
        lcdc_hx8347d_pdata = pdev->dev.platform_data;
        return 0;
    }
    msm_fb_add_device(pdev);
    return 0;
}

static struct platform_driver this_driver = {
    .probe  = hx8347d_probe,
    .driver = {
    	.name   = lCD_DRIVER_NAME,
    },
};

static struct msm_fb_panel_data hx8347d_panel_data = {
    .on = hx8347d_panel_on,
    .off = hx8347d_panel_off,
    .set_backlight = hx8347d_set_backlight,
    .set_contrast = hx8347d_panel_set_contrast,
};

static struct platform_device this_device = {
    .name   = lCD_DRIVER_NAME,
    .id	= 1,
    .dev	= {
    	.platform_data = &hx8347d_panel_data,
    }
};

static int __init hx8347d_panel_init(void)
{
    int ret;
    struct msm_panel_info *pinfo;

    lcd_panel_qvga = lcd_panel_probe();
	
    if((LCD_HX8347D_TRULY_QVGA != lcd_panel_qvga) && \
		(LCD_HX8347D_INNOLUX_QVGA!= lcd_panel_qvga) && \
        (msm_fb_detect_client(lCD_DRIVER_NAME))
      )
    {
        return 0;
    }


    ret = platform_driver_register(&this_driver);
    if (ret)
        return ret;

    pinfo = &hx8347d_panel_data.panel_info;
    pinfo->xres = 240;
    pinfo->yres = 320;
    pinfo->type = LCDC_PANEL;
    pinfo->pdest = DISPLAY_1;
    pinfo->wait_cycle = 0;
    pinfo->bpp = 18;
    pinfo->fb_num = 2;
    pinfo->bl_max = LCD_MAX_BACKLIGHT_LEVEL;
    pinfo->bl_min = LCD_MIN_BACKLIGHT_LEVEL;

    pinfo->clk_rate = 6125000;  /*for QVGA pixel clk*/   
    pinfo->lcdc.h_back_porch = 2;
    pinfo->lcdc.h_front_porch = 2;
    pinfo->lcdc.h_pulse_width = 2;
    pinfo->lcdc.v_back_porch = 2;
    pinfo->lcdc.v_front_porch = 2;
    pinfo->lcdc.v_pulse_width = 2;

    pinfo->lcdc.border_clr = 0;     /* blk */
    pinfo->lcdc.underflow_clr = 0xff;       /* blue */
    pinfo->lcdc.hsync_skew = 0;

    ret = platform_device_register(&this_device);
    if (ret)
        platform_driver_unregister(&this_driver);

    return ret;
}

module_init(hx8347d_panel_init);
