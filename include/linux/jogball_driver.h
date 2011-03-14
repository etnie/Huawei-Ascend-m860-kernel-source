#ifndef _JOGBALL_DRIVER_H_
#define _JOGBALL_DRIVER_H_

#include <linux/irqreturn.h>
#include <linux/ptrace.h>
#include <linux/input.h>


struct jogball_button {	
	int code;		
	int gpio;
	int active_low;
	char *desc;
	int type;			
};

struct jogball_platform_data {
	struct jogball_button *buttons;
	int nbuttons;
	int gpio_ctl;
};

struct gpio_button_data {
	struct jogball_button *button;
	struct input_dev *input;
};

struct jogball_drvdata {
	struct input_dev *input;
	struct gpio_button_data data[0];
       struct work_struct jogball_work;
};
typedef enum
{
    GPIO_LOW_VALUE  = 0,
    GPIO_HIGH_VALUE = 1
} gpio_value_type;


#define ACTIVE_HIGH 1
#define ACTIVE_LOW 0


void jogball_do_work(struct work_struct *work);

int jogball_up_isr_start(struct platform_device *pdev);
int jogball_down_isr_start(struct platform_device *pdev);
int jogball_left_isr_start(struct platform_device *pdev);
int jogball_right_isr_start(struct platform_device *pdev);
int jogball_scan_handle(struct platform_device *pdev);

#endif

