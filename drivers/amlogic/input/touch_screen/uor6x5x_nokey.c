#ifndef CONFIG_TOUCHSCREEN_TS_KEY

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/serio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/poll.h>
#include <linux/spinlock.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/gpio.h>

#include <linux/i2c/uor6x5x.h>
//#define DEBUG

#define TRUE	1
#define FALSE 	0

#define InitX		0x00
#define InitY		0x10
#define MSRX_2T         0x40
#define MSRY_2T         0x50
#define MSRX_1T         0xC0
#define MSRY_1T         0xD0
#define MSRZ1_1T        0xE0
#define MSRZ2_1T        0xF0


//#define	GESTURE_IN_DRIVER//!!! Turn on Gesture Detection in Driver !!!
#define Tap				1	//Tap
#define RHorizontal		3	//Right horizontal
#define LHorizontal		4	//Left horizontal
#define UVertical			5	//Up vertical
#define DVertical			6	//Down vertical
#define RArc				7	//Right arc
#define LArc				8	//Left arc
#define CWCircle			9	//Clockwise circle
#define CCWCircle		10	//Counter-clockwise circle
#define RPan				11	//Right pan
#define LPan				12	//Left pan
#define DPan				13	//Right pan
#define UPan				14	//Left pan
#define PressTap			15	//Press and tap
#define PinchIn			16	//Pinch in
#define PinchOut			17	//Pinch out


#define R_Threshold 	       3500
#define R_Threshold2_LOW 	    1000	
//#define R_Threshold2_HIGH 	    800

//#define FIXED_PRESSURE_VALUE   //pressure use fixed value
#define MIN_PRESSURE_REPORT_VALUE   0

#define TOUCH_UNKNOW 0
#define ZERO_TOUCH  (1<<1)
#define ONE_TOUCH   (1<<2)
#define TWO_TOUCH   (1<<3)

#define PENDING_ONE_TOUCH (1<<8)
#define PENDING_TWO_TOUCH (1<<9)

#define DT_LOW			60
#define DT_HIGH			70
#define DT_HIGH2		100

#define WORK_QUEUE_DELAY 1
#define TWO_POINT_WORK_QUEUE_DELAY 10

#define ZERO_TOUCH_AFTER_TOUCHED_FILTER		0
#define FIRST_ONE_TOUCH_AFTER0_FILTER	10
#define FIRST_ONE_TOUCH_AFTER1_FILTER	(FIRST_ONE_TOUCH_AFTER0_FILTER-3)
#define FIRST_ONE_TOUCH_AFTER2_FILTER	200

#define FIRST_TWO_TOUCH_AFTER0_FILTER	3
#define FIRST_TWO_TOUCH_AFTER1_FILTER	3
#define FIRST_TWO_TOUCH_AFTER2_FILTER	FIRST_TWO_TOUCH_AFTER1_FILTER
#define JITTER_THRESHOLD_TWO_TOUCH	128
#define JITTER_THRESHOLD_OUTER      250
#define JITTER_THRESHOLD_CENTER     0
#define TWO_TOUCH_DELTA_DISTANCE_MIN     1500

#define TWO_TOUCH_DISTANCE_DISCREASE_SHIFT     6


#define ONE_TOUCH_JITTER_LOW_AVERAGE    80
#define ONE_TOUCH_JITTER_HIGH_AVERAGE  200
#define ONE_TOUCH_JITTER_HIGH_DROP     600

#define MAX_PENDING_POINT   6

#define NumberFilter		6
#define NumberDrop			4	//This value must not bigger than (NumberFilter-1)


typedef signed char VINT8;
typedef unsigned char VUINT8;
typedef signed short VINT16;
typedef unsigned short VUINT16;
typedef unsigned long VUINT32;
typedef signed long VINT32;


/**
 * This filter works as follows: we keep track of latest N samples,
 * and average them with certain weights. The oldest samples have the
 * least weight and the most recent samples have the most weight.
 * This helps remove the jitter and at the same time doesn't influence
 * responsivity because for each input sample we generate one output
 * sample; pen movement becomes just somehow more smooth.
 */

#define NR_SAMPHISTLEN  4

/* To keep things simple (avoiding division) we ensure that
 * SUM(weight) = power-of-two. Also we must know how to approximate
 * measurements when we have less than NR_SAMPHISTLEN samples.
 */
static const unsigned char weight [NR_SAMPHISTLEN - 1][NR_SAMPHISTLEN + 1] =
{
    /* The last element is pow2(SUM(0..3)) */
    { 5, 3, 0, 0, 3 },  /* When we have 2 samples ... */
    { 8, 5, 3, 0, 4 },  /* When we have 3 samples ... */
    { 6, 4, 3, 3, 4 },  /* When we have 4 samples ... */
};

/*
struct ts_hist {
    int x;
    int y;
    int dx;
    int dy;
    unsigned int p;
};*/

struct ts_sample {
	int		x;
	int		y;
    int     dx;
    int     dy;	
	unsigned int	p;
};

struct tslib_dejitter {
    /*int delta;
    int x;
    int y;*/
    //int down;

    struct ts_sample old;
    struct ts_sample curr;
    struct ts_sample delta;

    int nr;
    int head;
    struct ts_sample hist[NR_SAMPHISTLEN];
};

static int sqr (int x)
{
    return x * x;
}

static void average (struct tslib_dejitter *djt, struct ts_sample *samp)
{
    const unsigned char *w;
    int sn = djt->head;
    int i, x = 0, y = 0;
    unsigned int p = 0;

        w = weight [djt->nr - 2];

    for (i = 0; i < djt->nr; i++) {
        x += djt->hist [sn].x * w [i];
        y += djt->hist [sn].y * w [i];
        p += djt->hist [sn].p * w [i];
        sn = (sn - 1) & (NR_SAMPHISTLEN - 1);
    }

    samp->x = x >> w [NR_SAMPHISTLEN];
    samp->y = y >> w [NR_SAMPHISTLEN];
    samp->p = p >> w [NR_SAMPHISTLEN];
#if 0
    printk("DEJITTER----------------> %d %d %d\n",
        samp->x, samp->y, samp->p);
#endif
}

/*
void swap(int *ary, int i, int j)
{
   int t;
   t = ary[i]; ary[i] = ary[j]; ary[j] = t;
}


int middle(int *ary, int num)
{
    int low, high, elem, begin, end;


    begin = 0; end = num;
    while( begin < end ) {
        elem = ary[begin];
        low = begin;
        high = end;
        while(ary[++low] < elem && low < high);
        while(ary[--high] >; elem && high >;= low);
        while(low < high) {
            swap(ary, low, high);
            while(ary[++low] < elem && low < high);
            while(ary[--high] >; elem && high >;= low);
        }
        swap(ary, begin, high);


        if(high >; num >>1)
            end = high;
        else if(high < (num >>1))
            begin = high + 1;
        else
            break;
    }

    return ary[high];
}*/

static void average2 (struct tslib_dejitter *djt)
{
    int nr = djt->nr;
    int sn = djt->head;
    int i;
    struct ts_sample avg;
    struct ts_sample min,max;

    //save old value
    memcpy(&djt->old,&djt->curr,sizeof(djt->old));
    memset(&djt->delta,0,sizeof(djt->delta));

    if(!nr){
        memset(&djt->curr,0,sizeof(djt->curr));
        return;
    }

    //caculte average
    memcpy(&avg,&djt->hist [sn],sizeof(avg));
    memcpy(&max,&djt->hist [sn],sizeof(max));
    memcpy(&min,&djt->hist [sn],sizeof(min));

    for (i = 0; i < nr-1; i++) {
        sn = (sn - 1) & (NR_SAMPHISTLEN - 1);

        avg.x += djt->hist [sn].x;
        avg.y += djt->hist [sn].y;
        avg.dx += djt->hist [sn].dx;
        avg.dy += djt->hist [sn].dy;        
        avg.p += djt->hist [sn].p;

        if(max.x < djt->hist [sn].x)  max.x = djt->hist [sn].x;
        if(max.y < djt->hist [sn].y)  max.y = djt->hist [sn].y;
        if(max.dx < djt->hist [sn].dx)  max.dx = djt->hist [sn].dx;
        if(max.dy < djt->hist [sn].dy)  max.dy = djt->hist [sn].dy;
        if(max.p < djt->hist [sn].p)  max.p = djt->hist [sn].p;        

        if(min.x > djt->hist [sn].x)  min.x = djt->hist [sn].x;
        if(min.y > djt->hist [sn].y)  min.y = djt->hist [sn].y;
        if(min.dx > djt->hist [sn].dx)  min.dx = djt->hist [sn].dx;
        if(min.dy > djt->hist [sn].dy)  min.dy = djt->hist [sn].dy;
        if(min.p > djt->hist [sn].p)  min.p = djt->hist [sn].p;

        
    }

    if(nr > 2){  //more than 2, delete max and min
        avg.x =avg.x - min.x -max.x;
        avg.y =avg.y - min.y -max.y;
        avg.dx =avg.dx - min.dx -max.dx;
        avg.dy =avg.dy - min.dy -max.dy;
        avg.p =avg.p - min.p -max.p;
        nr -= 2;
    }

    if( nr >2 ){  //more than 2, use divide
        nr -= 2;
        avg.x /= nr;
        avg.y /= nr;
        avg.dx /= nr;
        avg.dy /= nr;
        avg.p /= nr;
    }else if( nr ==2 ){  //equal 2, use shift
        avg.x >>= 1;
        avg.y >>= 1;
        avg.dx >>= 1;
        avg.dy >>=1;
        avg.p >>=1;
    }

    //save average to new
    memcpy(&djt->curr,&avg,sizeof(djt->curr));

    //caculte delta
    if(djt->nr > 1){
        djt->delta.x = djt->curr.x - djt->old.x;
        djt->delta.y = djt->curr.y - djt->old.y;
        djt->delta.dx = djt->curr.dx - djt->old.dx;
        djt->delta.dy = djt->curr.dy - djt->old.dy;
        djt->delta.p = djt->curr.p - djt->old.p;
    }
}


static int dejitter(struct tslib_dejitter *djt, struct ts_sample *samp)
{
    struct ts_sample *s;
    int count = 0;

    s = samp;
    if (s->p == 0) {
        /*
         * Pen was released. Reset the state and
         * forget all history events.
         */
        djt->nr = 0;
        samp [count++] = *s;
        return count;
    }

    /* If the pen moves too fast, reset the backlog. */
#if 0
    if (djt->nr) {
        int prev = (djt->head - 1) & (NR_SAMPHISTLEN - 1);
        if (sqr (s->x - djt->hist [prev].x) +
            sqr (s->y - djt->hist [prev].y) > djt->delta) {
#ifdef DEBUG
            printk ("DEJITTER: pen movement exceeds threshold\n");
#endif
            djt->nr = 0;
        }
    }
#endif
    djt->hist[djt->head].x = s->x;
    djt->hist[djt->head].y = s->y;
    djt->hist[djt->head].p = s->p;
    if (djt->nr < NR_SAMPHISTLEN)
        djt->nr++;

    /* We'll pass through the very first sample since
     * we can't average it (no history yet).
     */
    if (djt->nr == 1)
        samp [count] = *s;
    else {
        average (djt, samp + count);
    }
    count++;

    djt->head = (djt->head + 1) & (NR_SAMPHISTLEN - 1);

    return count;
}


struct uor6x5x_touch_screen_struct {
	struct i2c_client *client;
	struct input_dev *dev;
	struct input_dev *dev_single;

    struct tslib_dejitter djt_xy;
    struct tslib_dejitter djt_distance;
    struct tslib_dejitter pointer;

    int irq_disabled;

    int pendown;

    int distance;

    int direction;
    int direction_distance;

    struct ts_sample pending_sample_one[MAX_PENDING_POINT];
    int pending_one_touch;
    int pending_one_touch_ns;
    int pending_one_touch_nr;
    
    struct ts_sample pending_sample_two[MAX_PENDING_POINT];
    int pending_two_touch;
    int pending_two_touch_ns;
    int pending_two_touch_nr;
    struct ts_sample first_two_touch_sample;

	int pending_zero_touch_nr;
	
	struct uor6x5x_platform_data *pdata;
};

static struct uor6x5x_touch_screen_struct ts;

static int uor_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int uor_remove(struct i2c_client *client);
static int uor_suspend(struct device *dev);
static int uor_resume(struct device *dev);

static const struct i2c_device_id uor_idtable[] = {
	{ "uor6x5x", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, uor_idtable);


static struct dev_pm_ops uor_i2c_pm_ops = {
	.suspend	= uor_suspend,
	.resume	= uor_resume,
};


static struct i2c_driver uor_i2c_driver = {
        .driver = {
            .name   = "uor6x5x",
            .owner  = THIS_MODULE,
            .pm = &uor_i2c_pm_ops,
        },
        .id_table	= uor_idtable,      
        .probe          = uor_probe,
        .remove         = __devexit_p(uor_remove),
};

static int reg_read_n(__u8 reg, __u8 *data, int len)
{
    struct uor6x5x_touch_screen_struct *t = &ts;

	struct i2c_msg msgs0[] = {
		{
			.addr	 	= t->client->addr,
			.flags  	= 0,
			.len		= 1,
			.buf		= &reg,
		},
		{
			.addr	 	= t->client->addr,
			.flags  	= 0,
			.len		= 0,
			.buf		= NULL,
		}
	};
	struct i2c_msg msgs1[] = {
		{
			.addr	 	= t->client->addr,
			.flags  	= 0,
			.len		= 1,
			.buf		= &reg,
		},
		{
			.addr	 	= t->client->addr,
			.flags  	= I2C_M_RD,
			.len		= len,
			.buf		= data,
		}
	};	
	if (i2c_transfer(t->client->adapter, msgs0, ARRAY_SIZE(msgs0)) != ARRAY_SIZE(msgs0)){
		printk(KERN_ERR "Uor6x5x i2c access failed #1\n");
		return -1;
	}

	if (i2c_transfer(t->client->adapter, msgs1, ARRAY_SIZE(msgs1)) != ARRAY_SIZE(msgs1)){
		printk(KERN_ERR "Uor6x5x i2c access failed #2\n");
		return -1;
	}

    return 0;
}

static int uor_get_pendown_state(struct uor6x5x_touch_screen_struct *t)
{
    int state = 0;

    if (t->pdata && t->pdata->get_irq_level)
        state = t->pdata->get_irq_level();

    return state;
}

int  XYFilter(int *xFilter, int *yFilter, int Num,int Drop){
	unsigned int i,SumTempX=0,SumTempY=0;
	int Dp,checkSmx,checkSmy,checkBgx,checkBgy;
	int SmX =0, SmY = 0;
	int LaX = 0, LaY = 0;
	int SmInX = 0, SmInY = 0;
	int LaInX = 0, LaInY =0;

	if( (Num <=2) && (Drop > (Num-1)) )
		return FALSE; // not enough to sample
		
	for(i=0;i<Num;i++){
		SumTempX += xFilter[i];
		SumTempY += yFilter[i];
	}
	
	Dp = Drop;

	checkSmx = 0;
	checkSmy = 0;
	checkBgx = 0;
	checkBgy = 0;
	while(Dp>0){
		SmX = 0x0FFF;SmY = 0x0FFF;
		LaX = 0x0;LaY = 0x0;
		SmInX = 0;SmInY = 0;
		LaInX = 0;LaInY =0;
		for(i =  0; i < Num; i++){
			if(checkSmx&(1<<i)){
			}else if(SmX > xFilter[i]){
				SmX = xFilter[i];
				SmInX= i;
			}
			if(checkSmy&(1<<i)){
			}else if(SmY > yFilter[i]){
				SmY = yFilter[i];
				SmInY = i;
			}
			
			if(checkBgx&(1<<i)){
			}else if(LaX < xFilter[i]){
				LaX = xFilter[i];
				LaInX = i;
			}
			
			if(checkBgy&(1<<i)){
			}else if(LaY < yFilter[i]){
				LaY = yFilter[i];
				LaInY = i;
			}
		}
		if(Dp){
			SumTempX-= xFilter[SmInX];
			SumTempX-= xFilter[LaInX];
			SumTempY-= yFilter[SmInY];
			SumTempY-= yFilter[LaInY];
			Dp -= 2;
			//printk(KERN_ERR "in filter :SmInX %d,LaInX %d, SmInY %d , LaInY %d!!!\n", SmInX,LaInX, SmInY, LaInY);
		}
		checkSmx |= 1<<SmInX;
		checkSmy |= 1<<SmInY;
		checkBgx |= 1<<LaInX;
		checkBgy |= 1<<LaInY;
	}
	
	xFilter[0] = SumTempX/(Num-Drop);
	yFilter[0] = SumTempY/(Num-Drop);
	
	return TRUE;
}

VINT8 uor_reset(void)
{

	u8   i,icdata[2];
	u32   Dx_REF,Dy_REF,Dx_Check,Dy_Check;
	int		  TempDx[NumberFilter],TempDy[NumberFilter];

    int ret = 0;

	for(i=0;i<NumberFilter;i++){
		ret = reg_read_n(InitX,icdata,2);
    	if(ret)
    	    return ret;		
		TempDx[i] = (icdata[0]<<4 | icdata[1]>>4);
        ret = reg_read_n(InitY,icdata,2);
    	if(ret)
    	    return ret;        
		TempDy[i] = (icdata[0]<<4 | icdata[1]>>4);
		//printk(KERN_ERR "filter test:#%d (x,y)=(%d,%d) !!!\n", i,TempDx[i], TempDy[i]);
	}
	XYFilter(TempDx,TempDy,NumberFilter,2);
    Dx_REF = TempDx[0];
    Dy_REF = TempDy[0];
	//printk(KERN_ERR "filter result:(x,y)=(%d,%d) !!!\n", Dx_REF, Dy_REF);
	
	i = 0;
	do{

		ret = reg_read_n(InitX,icdata,2);
    	if(ret)
    	    return ret;		
		Dx_Check = abs((icdata[0]<<4 | icdata[1]>>4) - Dx_REF);
		ret = reg_read_n(InitY,icdata,2);
    	if(ret)
    	    return ret;		
		Dy_Check = abs((icdata[0]<<4 | icdata[1]>>4) - Dy_REF);

		i++;

		if(i>NumberFilter)
			return -1;

	}while(Dx_Check > 4 || Dy_Check > 4);

	return ret;
}


static struct workqueue_struct *queue = NULL;
static struct delayed_work work;

static void uor_cache_reset(struct uor6x5x_touch_screen_struct *t)
{
    t->pendown = ZERO_TOUCH;
    
    t->pending_zero_touch_nr = t->pending_one_touch_nr = t->pending_two_touch_nr = 0;
    t->pending_one_touch_ns=t->pending_two_touch_ns = 0;
    t->pending_one_touch=t->pending_two_touch=0;
    
    t->distance = 0;
    t->direction=0;
    t->direction_distance = 0;

    memset(&t->djt_xy,0,sizeof(t->djt_xy));
    memset(&t->djt_distance,0,sizeof(t->djt_distance));
    memset(&t->pointer,0,sizeof(t->pointer));
    memset(&t->pending_sample_one,0,sizeof(t->pending_sample_one));
    memset(&t->pending_sample_two,0,sizeof(t->pending_sample_two));
    memset(&t->first_two_touch_sample,0,sizeof(t->first_two_touch_sample));
}


static int uor_read_data(struct ts_sample *s)
{

	u8 data[16];
    int x,y,dx,dy,z1,z2,rt;

    int ret = 0;    

	memset(data, 0, sizeof(data));
	ret = reg_read_n(MSRX_1T, data, 2);
	if(ret)
	    return ret;
	x = data[0]; 
	x <<= 4;
	x |= (data[1]>>4);
	
	ret = reg_read_n(MSRY_1T,  data, 2);
	if(ret)
	    return ret;	
	y = data[0]; 
	y <<= 4;
	y |= (data[1]>>4);
	
	ret = reg_read_n(MSRX_2T,  data, 3);
	if(ret)
	    return ret;	
	dx = data[2];
	
	ret = reg_read_n(MSRY_2T,  data, 3);
	if(ret)
	    return ret;	
	dy = data[2];	

    ret = reg_read_n(MSRZ1_1T,  data, 2);
	if(ret)
	    return ret;    
    z1 = data[0]; 
    z1 <<=4;
    z1 |= (data[1]>>4);
    
    ret = reg_read_n(MSRZ2_1T, data, 2);
	if(ret)
	    return ret;    
    z2 = data[0]; 
    z2 <<=4;
    z2 |= (data[1]>>4);
    
    if(z1 ==0) {
        z1 =1;//avoid divde by zero
    }
    rt =(abs(((z2*x)/z1-x)))>>2;
	
    s->x = x;
    s->y = y;
    s->dx = dx;
    s->dy = dy;
    s->p = rt;

    return ret;
}


static int uor_report_pending_points(struct uor6x5x_touch_screen_struct *t,int flags)
{
    int out,outx,outy,outp;
    int distance;

    int ns,nr;

    int ret = 0;

    if(flags & PENDING_TWO_TOUCH){
#if defined(DEBUG)                 
        printk(KERN_ERR "report pending two point %d %d\n",t->pending_two_touch_ns,t->pending_two_touch_nr);                      
#endif
        if(!t->first_two_touch_sample.p){
            t->first_two_touch_sample.x = (t->pdata->abs_xmax>>1);
            t->first_two_touch_sample.y = (t->pdata->abs_ymax>>1);
            t->first_two_touch_sample.p = 1;
        }

        ns=t->pending_two_touch_ns + MAX_PENDING_POINT - t->pending_two_touch_nr;
        if(ns >= MAX_PENDING_POINT)
            ns-= MAX_PENDING_POINT;

        nr = t->pending_two_touch_nr;
        while(nr--){
#if defined(DEBUG)        
            printk("M(%d)(%d,%d)\n",ns,t->pending_sample_two[ns].dx,t->pending_sample_two[ns].dy);
#endif        
            distance = (t->pending_sample_two[ns].dx+t->pending_sample_two[ns].dy)>>1;

            if(distance > t->first_two_touch_sample.x)
                distance=t->first_two_touch_sample.x;
            if(distance > t->first_two_touch_sample.y)
                distance=t->first_two_touch_sample.y;

	        outx = t->first_two_touch_sample.x - distance;
	        outy = t->first_two_touch_sample.y - distance;
	        if(t->pdata->convert){
	        	out = t->pdata->convert(outx, outy);
	        	outx = out >> 16;
	        	outy = out & 0xffff;
	        }
#if !defined(FIXED_PRESSURE_VALUE)
	        outp = t->pdata->abs_pmax-t->pdata->abs_pmin-t->pending_sample_two[ns].p;
	        if(outp < MIN_PRESSURE_REPORT_VALUE)
#endif
	            outp = MIN_PRESSURE_REPORT_VALUE;

#if defined(DEBUG)         		        
            printk(KERN_ERR "M1(%d %d %d)\n",outx,outy,outp);
#endif             
            input_report_abs(t->dev, ABS_MT_TOUCH_MAJOR, outp);           
            input_report_abs(t->dev, ABS_MT_POSITION_X, outx);
            input_report_abs(t->dev, ABS_MT_POSITION_Y, outy);
            input_mt_sync(t->dev);
                                
            outx = t->first_two_touch_sample.x + distance;
            outy = t->first_two_touch_sample.y + distance;
            if(t->pdata->convert){
                out = t->pdata->convert(outx, outy);
                outx = out >> 16;
                outy = out & 0xffff;
            }
            //second point
#if defined(DEBUG) 
            printk(KERN_ERR "M2(%d %d %d)\n",outx,outy,outp);
#endif                        
            input_report_abs(t->dev, ABS_MT_TOUCH_MAJOR, outp);
            //input_report_abs(t->dev, ABS_MT_WIDTH_MAJOR, 600+press);
            input_report_abs(t->dev, ABS_MT_POSITION_X, outx);
            input_report_abs(t->dev, ABS_MT_POSITION_Y, outy);
            input_mt_sync(t->dev);
            
            input_sync(t->dev);

            ns++;
            if(ns == MAX_PENDING_POINT)
                ns-=MAX_PENDING_POINT;
            msleep(1);
        }

        ret |= TWO_TOUCH;
    }

    if(flags & PENDING_ONE_TOUCH){
#if defined(DEBUG)                 
        printk(KERN_ERR "report pending one point %d %d\n",t->pending_one_touch_ns,t->pending_one_touch_nr);
#endif
        //input_report_key(t->dev_single, BTN_TOUCH, 1);
        
        ns=t->pending_one_touch_ns + MAX_PENDING_POINT - t->pending_one_touch_nr;
        if(ns >= MAX_PENDING_POINT)
            ns-= MAX_PENDING_POINT;

        nr = t->pending_one_touch_nr;
        while(nr--){

            outx = t->pending_sample_one[ns].x;
            outy = t->pending_sample_one[ns].y;
            if(t->pdata->convert){
                out = t->pdata->convert(outx, outy);
                outx = out >> 16;
                outy = out & 0xffff;
            }
#if !defined(FIXED_PRESSURE_VALUE)
	        outp = t->pdata->abs_pmax-t->pdata->abs_pmin-t->pending_sample_one[ns].p;
	        if(outp < MIN_PRESSURE_REPORT_VALUE)
#endif
	            outp = MIN_PRESSURE_REPORT_VALUE;            
#if defined(DEBUG)                         
            printk(KERN_ERR "(%d %d %d)\n",outx,outy,t->pending_sample_one[ns].p);
#endif                        
            input_report_abs(t->dev_single, ABS_X, outx);
            input_report_abs(t->dev_single, ABS_Y, outy);
            input_report_abs(t->dev_single, ABS_PRESSURE,outp);
            input_sync(t->dev_single);                        

            ns++;
            if(ns == MAX_PENDING_POINT)
                ns-=MAX_PENDING_POINT;
            msleep(1);

        }
        ret |= ONE_TOUCH;
    }

    return ret;
}


int inc_one_touch_pending_point(struct uor6x5x_touch_screen_struct *t,struct ts_sample *s)
{
    t->pending_sample_one[t->pending_one_touch_ns].x = s->x;
    t->pending_sample_one[t->pending_one_touch_ns].y = s->y;
    t->pending_sample_one[t->pending_one_touch_ns].p = s->p;
    
    t->pending_one_touch++;
    t->pending_one_touch_ns++;
    if(t->pending_one_touch_ns == MAX_PENDING_POINT)
        t->pending_one_touch_ns=0;
    if(t->pending_one_touch_nr < MAX_PENDING_POINT)
        t->pending_one_touch_nr++; 

    return (t->pending_one_touch_nr > 0);
}

int inc_two_touch_pending_point(struct uor6x5x_touch_screen_struct *t,struct ts_sample *s)
{
    t->pending_sample_two[t->pending_two_touch_ns].x = s->x;
    t->pending_sample_two[t->pending_two_touch_ns].y = s->y;
    t->pending_sample_two[t->pending_two_touch_ns].dx = s->dx;
    t->pending_sample_two[t->pending_two_touch_ns].dy = s->dy;
    t->pending_sample_two[t->pending_two_touch_ns].p = s->p;

    t->pending_two_touch++;
    t->pending_two_touch_ns++;
    if(t->pending_two_touch_ns == MAX_PENDING_POINT)
        t->pending_two_touch_ns=0;
    if(t->pending_two_touch_nr < MAX_PENDING_POINT)
        t->pending_two_touch_nr++;
    
    return (t->pending_two_touch_nr > 1);
}

int dec_one_touch_pending_point(struct uor6x5x_touch_screen_struct *t)
{
    if(t->pending_one_touch_ns==0)
        t->pending_one_touch_ns = MAX_PENDING_POINT-1;
    else
        t->pending_one_touch_ns--;
    if(t->pending_one_touch_nr) t->pending_one_touch_nr--;
    if(t->pending_one_touch) t->pending_one_touch--;

    return !(t->pending_one_touch_nr > 0);
}

int dec_two_touch_pending_point(struct uor6x5x_touch_screen_struct *t)
{
    if(t->pending_two_touch_ns==0)
        t->pending_two_touch_ns = MAX_PENDING_POINT-1;
    else
        t->pending_two_touch_ns--;
    if(t->pending_two_touch_nr) t->pending_two_touch_nr--;
    if(t->pending_two_touch) t->pending_two_touch--;

    return !(t->pending_two_touch_nr > 1);
}

void clr_one_touch_pending_point(struct uor6x5x_touch_screen_struct *t)
{
    t->pending_one_touch = 0;
    t->pending_one_touch_ns=0;
    t->pending_one_touch_nr=0;
}

void clr_two_touch_pending_point(struct uor6x5x_touch_screen_struct *t)
{
    t->pending_two_touch=0;
    t->pending_two_touch_ns=0;
    t->pending_two_touch_nr=0;
}

static void uor_read_loop(struct work_struct *data)
{
    struct uor6x5x_touch_screen_struct *t = &ts;
    struct uor6x5x_platform_data *pdata = t->pdata;
    struct ts_sample sample,delta;
    int out,outx,outy,outp;
    int ntouch;

    int queue_time = WORK_QUEUE_DELAY;

    do{
        if(t->pendown==TOUCH_UNKNOW){   //hardware need be re-init
            printk(KERN_ERR "uor hw init ++\n");
            if (pdata->init_irq)
                pdata->init_irq();
            
            if(uor_reset() == 0){
                uor_cache_reset(t);
                printk(KERN_ERR "uor hw init -- \n");
            }else{
                printk(KERN_ERR "uor_reset failed\n");
                break;
            }
        }

        if(uor_get_pendown_state(t) || uor_read_data(&sample)!=0){
            ntouch =  ZERO_TOUCH;
        }else{
            t->pointer.head = (t->pointer.head + 1) & (NR_SAMPHISTLEN - 1);

            if (t->pointer.nr < NR_SAMPHISTLEN)
                t->pointer.nr++;

            memcpy(&t->pointer.hist[t->pointer.head],&sample,sizeof(sample));
            average2(&t->pointer);

            memcpy(&sample,&t->pointer.curr,sizeof(sample));
            memcpy(&delta,&t->pointer.delta,sizeof(delta));

            if(uor_get_pendown_state(t) || sample.p == 0 || sample.p > R_Threshold) {
            	ntouch =  ZERO_TOUCH;	
            }else if(   (((t->pendown & TWO_TOUCH) && (sample.dx> DT_HIGH || sample.dy > DT_HIGH))||
                            (sample.dx> DT_HIGH2 || sample.dy > DT_HIGH2)||
                            (sample.dx> DT_LOW && sample.dy > DT_LOW)) &&
                        (abs(t->pdata->abs_xmax-sample.x)>JITTER_THRESHOLD_OUTER)&&
                        (abs(t->pdata->abs_ymax-sample.y)>JITTER_THRESHOLD_OUTER)&&
                        (abs(sample.x-t->pdata->abs_xmin)>JITTER_THRESHOLD_OUTER)&&
                        (abs(sample.y-t->pdata->abs_ymin)>JITTER_THRESHOLD_OUTER)&&
                        (abs(sample.x-(t->pdata->abs_xmin>>1))>JITTER_THRESHOLD_CENTER)&&
                        (abs(sample.y-(t->pdata->abs_ymin>>1))>JITTER_THRESHOLD_CENTER)&&
                        (sample.p < R_Threshold2_LOW)) {
            	ntouch =  TWO_TOUCH;
            }else {
                ntouch = ONE_TOUCH;
            }
#if defined(DEBUG)        
            printk(KERN_ERR "%x(%x) touch: (x,y)=(%d,%d)(%d,%d) (dx,dy)=(%d,%d)(%d,%d) R %d\n", 
                ntouch,t->pendown,
                sample.x,sample.y,delta.x,delta.y, 
                sample.dx,sample.dy,delta.dx,delta.dy,
                sample.p);
#endif

        }

        if(ntouch == ONE_TOUCH || ntouch == TWO_TOUCH){	// pen down            
            t->pending_zero_touch_nr = 0;
        	if(ntouch == TWO_TOUCH){
                if((delta.dx > JITTER_THRESHOLD_TWO_TOUCH || delta.dy > JITTER_THRESHOLD_TWO_TOUCH)){
#if defined(DEBUG)                 
                    printk(KERN_ERR "jitter (dual)\n");
#endif
                    if(dec_one_touch_pending_point(t)){
                        t->pendown &= ~PENDING_ONE_TOUCH;
                    }
        	    }else{
    	            int dir,distance,delta,step;
                    struct ts_sample avg;

                    avg.x=sample.dx;
                    avg.y=sample.dy;
                    avg.p=sample.p;
                    dejitter(&t->djt_xy,&avg);
    	            
                    //distance = sample.dx*sample.dx + sample.dy*sample.dy;
                    distance=avg.x*avg.x + avg.y*avg.y;
                    avg.x=avg.y=distance;
                    dejitter(&t->djt_distance,&avg);
                    distance=avg.x;

                    t->distance = t->distance;
                    if(t->distance)
                        delta = abs(t->distance - distance);
                    else
                        delta = 0;

#if defined(DEBUG)    
                    printk("distance delta %d dis %d last_dis %d\n",
                        delta,distance,t->distance);
#endif

                    step = TWO_TOUCH_DELTA_DISTANCE_MIN;
                    if(delta && delta < step){
                        distance = t->distance;
                    }

                    if(distance != t->distance){  //not report the same point in two touch mode
                        t->distance = distance;

        		        distance>>=TWO_TOUCH_DISTANCE_DISCREASE_SHIFT;
        		        
                        if(((t->pendown & ZERO_TOUCH) && t->pending_two_touch < FIRST_TWO_TOUCH_AFTER0_FILTER)||
                            ((t->pendown & ONE_TOUCH) && t->pending_two_touch < FIRST_TWO_TOUCH_AFTER1_FILTER)||
                            ((t->pendown & TWO_TOUCH) && t->pending_two_touch < FIRST_TWO_TOUCH_AFTER2_FILTER)){
#if defined(DEBUG) 
                            printk(KERN_ERR "pending (dual) %d %d\n",t->pending_two_touch_ns,t->pending_two_touch_nr);
#endif
                            sample.dx = sample.dy = distance;
                            if(inc_two_touch_pending_point(t,&sample)){
                                t->pendown |= PENDING_TWO_TOUCH;
                                t->pendown &= ~PENDING_ONE_TOUCH;  //delete one touch pending
                            }

                            if(dec_one_touch_pending_point(t)){
                                t->pendown &= ~PENDING_ONE_TOUCH;
                            }
                        }else{
#if defined(DEBUG)                     
                            printk("TWO_TOUCH dir %d delta %d distance %d(%d)\n",
                                t->direction,distance - t->direction_distance,distance,t->direction_distance);
#endif
                            
                            if(!t->direction_distance){  //init direction
                                if(distance > 350)
                                    t->direction_distance= distance;
                            }else{
                                //compare direction
                                delta = distance - t->direction_distance;
                                if(delta < 0 && t->direction >=0)
                                    dir = -1;
                                else if(delta > 0 && t->direction <=0)
                                    dir = 1;
                                else
                                    dir = t->direction;
                                
                                if(dir != t->direction){
                                    if(!t->direction || (
                                        abs(delta) > ((t->direction_distance*2)>>2)&&
                                        abs(delta) > 100)){
                                        t->direction = dir;
                                        t->direction_distance= distance;
#if defined(DEBUG)                                          
                                        printk("change dir %d distance %d\n",
                                            t->direction,distance);
#endif
                                    }else{
                                        distance = t->direction_distance;
                                    }
                                }else{
                                    t->direction_distance =distance;
                                }
                            }
                            
                            //record base axis
                            if(!t->first_two_touch_sample.p){
                                t->first_two_touch_sample.x = sample.x;
                                t->first_two_touch_sample.y = sample.y;
                                t->first_two_touch_sample.dx = distance;
                                t->first_two_touch_sample.dy = distance;
                                t->first_two_touch_sample.p = sample.p;
                            }
                                

                            if(t->pending_two_touch_nr > FIRST_TWO_TOUCH_AFTER0_FILTER){  //drop some points
                                t->pending_two_touch_nr -= FIRST_TWO_TOUCH_AFTER0_FILTER;
                                uor_report_pending_points(t,t->pendown & PENDING_TWO_TOUCH);
                            }
                            //first point
                            if(distance > t->first_two_touch_sample.x)
                                distance=t->first_two_touch_sample.x;
                            if(distance > t->first_two_touch_sample.y)
                                distance=t->first_two_touch_sample.y;

            		        outx = t->first_two_touch_sample.x - distance;
            		        outy = t->first_two_touch_sample.y - distance;
            		        if(t->pdata->convert){
            		        	out = t->pdata->convert(outx, outy);
            		        	outx = out >> 16;
            		        	outy = out & 0xffff;
            		        }
#if !defined(FIXED_PRESSURE_VALUE)
                            outp = t->pdata->abs_pmax-t->pdata->abs_pmin-sample.p;
                            if(outp < MIN_PRESSURE_REPORT_VALUE)
#endif
                                outp = MIN_PRESSURE_REPORT_VALUE;
                            
                            input_report_abs(t->dev, ABS_MT_TOUCH_MAJOR, outp);           
                            input_report_abs(t->dev, ABS_MT_POSITION_X, outx);
                            input_report_abs(t->dev, ABS_MT_POSITION_Y, outy);
                            input_mt_sync(t->dev);
                                                
                            outx = t->first_two_touch_sample.x + distance;
                            outy = t->first_two_touch_sample.y + distance;
                            if(t->pdata->convert){
                                out = t->pdata->convert(outx, outy);
                                outx = out >> 16;
                                outy = out & 0xffff;
                            }
                            //second point
                            input_report_abs(t->dev, ABS_MT_TOUCH_MAJOR, outp);
                            //input_report_abs(t->dev, ABS_MT_WIDTH_MAJOR, 600+press);
                            input_report_abs(t->dev, ABS_MT_POSITION_X, outx);
                            input_report_abs(t->dev, ABS_MT_POSITION_Y, outy);
                            input_mt_sync(t->dev);
                            
                            input_sync(t->dev);

                            if(t->pendown&ONE_TOUCH){   //close single touch after multi touch is work            
                                input_report_key(t->dev_single, BTN_TOUCH, 0);
                                input_report_abs(t->dev_single, ABS_PRESSURE, 0);
                                input_sync(t->dev_single);   
                            }

                            t->pendown=TWO_TOUCH;
                            clr_one_touch_pending_point(t);
                        }
                    }
                }
            }else if(ntouch == ONE_TOUCH){
                if((t->pendown & ONE_TOUCH)&&
                    (delta.x > ONE_TOUCH_JITTER_HIGH_DROP || delta.y > ONE_TOUCH_JITTER_HIGH_DROP)){
#if defined(DEBUG)                     
                    printk(KERN_ERR "jitter (single)\n");
#endif
                    if(dec_two_touch_pending_point(t)){
                        t->pendown &= ~PENDING_TWO_TOUCH;
                    }
                }else if((t->pendown & PENDING_TWO_TOUCH)&&
                            ((sample.dx > DT_LOW && sample.dy > DT_LOW)||
                            (sample.dx > DT_HIGH || sample.dy > DT_HIGH))){
#if defined(DEBUG)                     
                    printk(KERN_ERR "jitter (single 2)\n");
#endif
                    if(dec_two_touch_pending_point(t)){
                        t->pendown &= ~PENDING_TWO_TOUCH;
                    }
                }else{
                    if(delta.x < ONE_TOUCH_JITTER_LOW_AVERAGE || 
                        delta.x > ONE_TOUCH_JITTER_HIGH_AVERAGE)
                        sample.x = sample.x - (delta.x>>1);
                        
                    if(delta.y < ONE_TOUCH_JITTER_LOW_AVERAGE || 
                        delta.y > ONE_TOUCH_JITTER_HIGH_AVERAGE)
                        sample.y = sample.y - (delta.y>>1);

                    if((t->pendown & TWO_TOUCH) && t->pending_one_touch < FIRST_ONE_TOUCH_AFTER2_FILTER){
#if defined(DEBUG) 
                        printk(KERN_ERR "ignore (single)\n");
#endif
                        if(dec_two_touch_pending_point(t)){
                            t->pendown &= ~PENDING_TWO_TOUCH;
                        }
                    }else if(((t->pendown & ZERO_TOUCH) && (t->pending_one_touch < FIRST_ONE_TOUCH_AFTER0_FILTER))||
                        ((t->pendown & ONE_TOUCH) && (t->pending_one_touch < FIRST_ONE_TOUCH_AFTER1_FILTER))){
#if defined(DEBUG) 
                        printk(KERN_ERR "pending (single) ns %d nr %d\n",t->pending_one_touch_ns,t->pending_one_touch_nr);
#endif
                        if(inc_one_touch_pending_point(t,&sample)){
                            t->pendown |= PENDING_ONE_TOUCH;  
                        }
                        if(dec_two_touch_pending_point(t)){
                            t->pendown &= ~PENDING_TWO_TOUCH;  
                        }                        
                    }else{

                        if (!(t->pendown & ONE_TOUCH)) {
                            if(t->pendown & TWO_TOUCH){
                                input_report_abs(t->dev, ABS_MT_TOUCH_MAJOR, 0);
                    	        input_mt_sync(t->dev);
                    	        input_sync(t->dev);
                            }
                            input_report_key(t->dev_single, BTN_TOUCH, 1);                            
                            uor_report_pending_points(t,t->pendown & PENDING_ONE_TOUCH);
                        }

                        outx = sample.x;
                        outy = sample.y;

                        if(t->pdata->convert){
                            out = t->pdata->convert(outx, outy);
                            outx = out >> 16;
                            outy = out & 0xffff;
                        }
#if !defined(FIXED_PRESSURE_VALUE)                        
                        outp = t->pdata->abs_pmax-t->pdata->abs_pmin-sample.p;
                        if(outp < MIN_PRESSURE_REPORT_VALUE)
#endif
                            outp = MIN_PRESSURE_REPORT_VALUE;
#if defined(DEBUG)
                        printk(KERN_ERR "ONE_TOUCH (%d,%d)\n", outx, outy);
#endif
            			input_report_abs(t->dev_single, ABS_X, outx);
            			input_report_abs(t->dev_single, ABS_Y, outy);
            			input_report_abs(t->dev_single, ABS_PRESSURE, outp);
                        input_sync(t->dev_single);

                        t->pendown = ONE_TOUCH;
                        clr_two_touch_pending_point(t);
                        t->direction=0;
                        t->direction_distance = 0;
                    }
                }
            }
        }else if(ntouch == ZERO_TOUCH){	// pen release
            int freeze_time = 0; 
#if defined(DEBUG)        
    	    printk("ZERO_TOUCH 0x%x %d %d\n",t->pendown,t->pending_zero_touch_nr,uor_get_pendown_state(t));
#endif
            if(t->pendown & (ONE_TOUCH|TWO_TOUCH|PENDING_ONE_TOUCH|PENDING_TWO_TOUCH)){
        		t->pending_zero_touch_nr++;
            	if(uor_get_pendown_state(t) && 
            	    (t->pending_zero_touch_nr > ZERO_TOUCH_AFTER_TOUCHED_FILTER)){
                	uor_reset();

                    if(t->pendown & PENDING_ONE_TOUCH){
                        input_report_key(t->dev_single, BTN_TOUCH, 1);
                    }
                    t->pendown |= uor_report_pending_points(t,t->pendown);
                	
                    if(t->pendown & TWO_TOUCH){
            	        input_report_abs(t->dev, ABS_MT_TOUCH_MAJOR, 0);
            	        input_mt_sync(t->dev);
            	        input_sync(t->dev);
            	        freeze_time =100;
                    }
                    
                    if(t->pendown & ONE_TOUCH){
                        input_report_abs(t->dev_single, ABS_PRESSURE, 0);
            			input_report_key(t->dev_single, BTN_TOUCH, 0);
            			input_sync(t->dev_single);
                    }
                    //reset filter parameters
                    uor_cache_reset(t);

                    if(freeze_time)
                        msleep(freeze_time);
                }
            }
        }else{
            printk(KERN_ERR "uor_read_loop(): n_touch state error !!!\n");
        }
#if !defined(DEBUG)
        if(t->pendown != ZERO_TOUCH){
            if(t->pendown & (TWO_TOUCH|PENDING_TWO_TOUCH))
                queue_time = TWO_POINT_WORK_QUEUE_DELAY;
        
            queue_delayed_work(queue, &work, queue_time);
            break;
        }
#endif        
    }while(t->pendown != ZERO_TOUCH && t->pendown != TOUCH_UNKNOW);

    if(t->pendown==TOUCH_UNKNOW){
        queue_delayed_work(queue, &work, HZ*2);
    }else if(t->pendown == ZERO_TOUCH){
        if(t->irq_disabled){
            enable_irq(t->client->irq);
            t->irq_disabled = 0;
        }
    }

    return;
}

static irqreturn_t uor_isr(int irq,void *dev_id)
{
    struct uor6x5x_touch_screen_struct *t = &ts;

#if defined(UOR_DIABLE_IRQ)
	struct i2c_client *client = (struct i2c_client *)dev_id;
#endif	
    if(uor_get_pendown_state(t)){//ting debounce
        return IRQ_HANDLED;
    }
#if defined(UOR_DIABLE_IRQ)
    t->irq_disabled = 1;
	disable_irq_nosync(client->irq);
#endif
	queue_delayed_work(queue, &work, WORK_QUEUE_DELAY);
	return IRQ_HANDLED;
}

static int uor_register_input(void)
{
    int ret;
    struct input_dev *	input_device;
    
    input_device = input_allocate_device();
    if (!input_device) {
    	printk(KERN_ERR "Unable to allocate the input device !!\n");
    	return -ENOMEM;
    }
    
    input_device->name = "UOR-touch";
    
    ts.dev = input_device;
    set_bit(EV_SYN, ts.dev->evbit);
    set_bit(EV_ABS, ts.dev->evbit);
    
    input_set_abs_params(ts.dev, ABS_MT_TOUCH_MAJOR, ts.pdata->abs_pmin, ts.pdata->abs_pmax, 0, 0);
    //input_set_abs_params(codec_ts_input, ABS_MT_WIDTH_MAJOR, 0, 1000, 0, 0);

    input_set_abs_params(ts.dev, ABS_MT_POSITION_X, ts.pdata->abs_xmin, ts.pdata->abs_xmax, 0, 0);
    input_set_abs_params(ts.dev, ABS_MT_POSITION_Y, ts.pdata->abs_ymin, ts.pdata->abs_ymax, 0, 0);
    
    ret = input_register_device(ts.dev);
    if (ret) {
    	input_free_device(ts.dev);
    	printk(KERN_ERR "%s: unabled to register input device, ret = %d\n", __FUNCTION__, ret);
    	return ret;
    }
    return 0;
}

static int uor_register_input_single(void)
{
    int ret;
    struct input_dev *	input_device;
    
    input_device = input_allocate_device();
    if (!input_device) {
    	printk(KERN_ERR "Unable to allocate the input device !!\n");
    	return -ENOMEM;
    }
    
    input_device->name = "UOR-touch-single";
    
    ts.dev_single = input_device;
    
    
	ts.dev_single->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	ts.dev_single->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	
    input_set_abs_params(ts.dev_single, ABS_X, ts.pdata->abs_xmin, ts.pdata->abs_xmax, 0, 0);
    input_set_abs_params(ts.dev_single, ABS_Y, ts.pdata->abs_ymin, ts.pdata->abs_ymax, 0, 0);
    input_set_abs_params(ts.dev_single, ABS_PRESSURE, ts.pdata->abs_pmin, ts.pdata->abs_pmax, 0, 0);
    
    ret = input_register_device(ts.dev_single);
    if (ret) {
    	input_free_device(ts.dev_single);
    	printk(KERN_ERR "%s: unabled to register input device, ret = %d\n", __FUNCTION__, ret);
    	return ret;
    }
    return 0;
}

static int __init uor_init(void)
{
	int ret = 0;
	//printk(KERN_ERR "uor.c: uor_init() !\n");

	memset(&ts, 0, sizeof(struct uor6x5x_touch_screen_struct));//init data struct ts

	ret = i2c_add_driver(&uor_i2c_driver);
	if(ret < 0) {
		printk(KERN_ERR "uor.c: i2c_add_driver() fail in uor_init()!\n");
		return ret;
	}	
	return ret;
}

static int __devinit uor_probe(struct i2c_client *client,
    const struct i2c_device_id *id)
{
    int err = 0;
    struct uor6x5x_platform_data *pdata = pdata = client->dev.platform_data;
    
    printk(KERN_ERR "uor_probe() !\n");
	
	if (!pdata) {
		dev_err(&client->dev, "platform data is required!\n");
		return -EINVAL;
	}
	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	        return -EIO;
	        
	queue = create_singlethread_workqueue("uor-touch-screen-read-loop");
	INIT_DELAYED_WORK(&work, uor_read_loop);

    if(!pdata->abs_xmax)
        pdata->abs_xmax=4096;
    if(!pdata->abs_ymax)
        pdata->abs_ymax=4096;
    if(!pdata->abs_pmax)
        pdata->abs_pmax=1000;
	
	if (pdata->init_irq)
		pdata->init_irq();
	ts.client = client; // save the client we get	
	ts.pdata = pdata;
    ts.pendown = TOUCH_UNKNOW;

	if (uor_register_input() < 0) {
		dev_err(&client->dev, "register input fail!\n");
		return -ENOMEM;
	}

	if (uor_register_input_single() < 0) {
		dev_err(&client->dev, "register input single fail!\n");
		return -ENOMEM;
	}	
	//printk(KERN_ERR "driver name is <%s>,client->irq is %d,INT_GPIO_0 is %d\n",client->dev.driver->name, client->irq, INT_GPIO_0);
	err = request_irq(client->irq, uor_isr, IRQF_TRIGGER_FALLING,client->dev.driver->name, client);
	if(err < 0){
		input_free_device(ts.dev);
		input_free_device(ts.dev_single);
		printk(KERN_ERR "uor.c: Could not allocate GPIO intrrupt for touch screen !!!\n");
		free_irq(client->irq, client);
		err = -ENOMEM;
		return err;	
	}
	
    //init chip
    if(uor_reset() == 0){
        uor_cache_reset(&ts);
        printk(KERN_ERR "uor hw init -- \n");
    }else{
        printk(KERN_ERR "uor_reset failed\n");
        queue_delayed_work(queue, &work, HZ*2);
    }
	
	printk(KERN_ERR "uor_probe ok!\n");
	return err;
}

int uor_suspend(struct device *dev)
{    
    return 0;
}
int uor_resume(struct device *dev)
{    
    ts.pendown = TOUCH_UNKNOW;
    queue_delayed_work(queue, &work, HZ);

    return 0;
}


static int __devexit uor_remove(struct i2c_client *client)
{
	free_irq(client->irq, client);
	return 0;
}

static void __exit uor_exit(void)
{
	i2c_del_driver(&uor_i2c_driver);
}


module_init(uor_init);
module_exit(uor_exit);

MODULE_DESCRIPTION("UOR Touchscreen driver");
MODULE_AUTHOR("Ming-Wei Chang <mingwei@uutek.com.tw>");
MODULE_LICENSE("GPL");

#endif