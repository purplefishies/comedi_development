/*
Driver: accesisa
Description: ACCES I/O PC104 Series Driver
Devices:
Author: James Damon
Updated: Tue, 13 May 2014 12:26:55 -0700
Status: experimental

This driver is a documented example on how Comedi drivers are
written.

Configuration Options:
  none
*/

#include <linux/comedidev.h>
/* #include <linux/pci.h> */
/* Imaginary registers for the imaginary board */

#define ACCESISA_SIZE 0

#define ACCESISA_START_AI_CONV	0
#define ACCESISA_AI_READ		0

#define ACCES_READ 0xff00
#define ACCES_WRITE 0x00ff


#define MAKE_ADDRESS(X,Y)  ((X << 16 ) && (Y))
#define BASE_ADDRESS(X)    ((0xffff) && (X) )

#define IRQ 1

/*
 * Board descriptions for two imaginary boards.  Describing the
 * boards in this way is optional, and completely driver-dependent.
 * Some drivers use arrays such as this, other do not.
 */
typedef struct accesisa_board_struct {
    const char *name;
    int ai_chans;
    int ai_bits;
    int have_dio;
    /* IRQ values */
    int clear_interupt_offset;
    int clear_interrupt_value;
    int enable_irq_offset;
    int disable_irq_offset;

    int num_dio_bits;
    int num_out_bits;

    int const *read_bases;      /* DI Reading */
    int num_read_bases;

    int const *do_bases;        /* DO Reading */
    int num_do_bases;

    int num_in_bits;
    int const *write_bases;
    int interrupt_status_offset;
    int num;
    int iosize;
} accesisa_board;

static const accesisa_board accesisa_boards[] = {
	{
	      name:	"accesio_idio16",
              iosize:8,
	      ai_chans:0,
	      ai_bits:0,
	      num_dio_bits:0,
              num_out_bits:16,
              clear_interupt_offset:MAKE_ADDRESS(ACCES_WRITE, 0x01 ),
              clear_interrupt_value:1,
              disable_irq_offset:MAKE_ADDRESS(ACCES_WRITE, 0x02 ),
              enable_irq_offset:MAKE_ADDRESS(ACCES_READ, 0x02 ),
              read_bases: (const int []){ MAKE_ADDRESS( ACCES_READ , 0x01), MAKE_ADDRESS(ACCES_READ , 0x05 ) },
              num_read_bases: 2,
              num_in_bits:16,
              write_bases:(const int []){ MAKE_ADDRESS( ACCES_WRITE, 0x01), MAKE_ADDRESS(ACCES_WRITE, 0x04 ) },
        },
	{
	      name:	"accesio_iiro16",
              iosize:8,
	      ai_chans:0,
	      ai_bits:0,
	      num_in_bits:16,
	      num_out_bits:16,
              clear_interupt_offset:MAKE_ADDRESS(ACCES_WRITE, 0x01 ),
              clear_interrupt_value:1,
              disable_irq_offset:MAKE_ADDRESS(ACCES_WRITE, 0x02 ),
              enable_irq_offset: MAKE_ADDRESS(ACCES_READ, 0x02 ),
              read_bases: (const int []){ MAKE_ADDRESS(ACCES_READ  , 0x00 ), /* First relay output */
                                          MAKE_ADDRESS(ACCES_READ  , 0x04 ), /* Second relay output */
                                          MAKE_ADDRESS(ACCES_READ  , 0x01 ), /* First Isolated input */
                                          MAKE_ADDRESS(ACCES_READ  , 0x05 )  /* Second Isolated input */
                                        },
              num_read_bases:4,
              write_bases:(const int []){ MAKE_ADDRESS( ACCES_WRITE, 0x00), MAKE_ADDRESS(ACCES_WRITE, 0x04 ) },
              
        },
        /* Must support 
         * counter
         * AO
         * AI
         * DO
         * DI
         */
	/* { */
	/*       name:	"accesio_aio16", */
        /*       iosize: 0x1E, */
	/*       ai_chans:0, */
	/*       ai_bits:16, */
	/*       num_dio_bits:16, */
        /*       clear_interupt_offset:MAKE_ADDRESS(ACCES_WRITE, 0x01 ), */
        /*       clear_interrupt_value:1, */
        /*       disable_irq_offset:MAKE_ADDRESS(ACCES_WRITE, 0x02 ), */
        /*       enable_irq_offset: MAKE_ADDRESS(ACCES_READ, 0x02 ), */
        /*       read_bases: (const int []){ MAKE_ADDRESS( ACCES_READ , 0x01), MAKE_ADDRESS(ACCES_READ , 0x05 ) }, */
        /*       write_bases:(const int []){ MAKE_ADDRESS( ACCES_WRITE, 0x00), MAKE_ADDRESS(ACCES_WRITE, 0x04 ) }, */
        /* } */
};

/* Comedi Constants and Ranges*/
static const comedi_lrange range_accesisa_aio_range = { 2, { BIP_RANGE(10) , UNI_RANGE(5)  }
};

/* This is used by modprobe to translate PCI IDs to drivers.  Should
 * only be used for PCI and ISA-PnP devices */
/* Please add your PCI vendor ID to comedidev.h, and it will be forwarded
 * upstream. */

/*
 * Useful for shorthand access to the particular board structure
 */
#define thisboard ((const accesisa_board *)dev->board_ptr)

/* this structure is for data unique to this hardware driver.  If
   several hardware drivers keep similar information in this structure,
   feel free to suggest moving the variable to the comedi_device struct.  */
typedef struct {
    int data;
    int base;
    /* would be useful for a PCI device */
    /* struct pci_dev *pci_dev; */
    /* Used for AO readback */
    lsampl_t ao_readback[2];
} accesisa_private;

/*
 * most drivers define the following macro to make it easy to
 * access the private structure.
 */
#define devpriv ((accesisa_private *)dev->private)

/*
 * The comedi_driver structure tells the Comedi core module
 * which functions to call to configure/deconfigure (attach/detach)
 * the board, and also about the kernel module that contains
 * the device code.
 */
static int accesisa_attach(comedi_device * dev, comedi_devconfig * it);
static int accesisa_detach(comedi_device * dev);
static comedi_driver driver_accesisa = {
      driver_name:"dummy",
      module:THIS_MODULE,
      attach:accesisa_attach,
      detach:accesisa_detach,
      board_name:&accesisa_boards[0].name,
      offset:sizeof(accesisa_board),
      num_names:sizeof(accesisa_boards) / sizeof(accesisa_board),
};

static int waveform_ai_cmdtest(comedi_device * dev, comedi_subdevice * s,
                               comedi_cmd * cmd);
static int waveform_ai_cmd(comedi_device * dev, comedi_subdevice * s);

static int accesisa_cmd_irq(comedi_device * dev, comedi_subdevice * s);

static int accesisa_ai_rinsn(comedi_device * dev, comedi_subdevice * s,
	comedi_insn * insn, lsampl_t * data);
static int accesisa_ao_winsn(comedi_device * dev, comedi_subdevice * s,
	comedi_insn * insn, lsampl_t * data);
static int accesisa_ao_rinsn(comedi_device * dev, comedi_subdevice * s,
	comedi_insn * insn, lsampl_t * data);
static int accesisa_dio_insn_bits(comedi_device * dev, comedi_subdevice * s,
	comedi_insn * insn, lsampl_t * data);
static int accesisa_dio_insn_config(comedi_device * dev, comedi_subdevice * s,
	comedi_insn * insn, lsampl_t * data);
static int accesisa_ai_cmdtest(comedi_device * dev, comedi_subdevice * s,
	comedi_cmd * cmd);
static int accesisa_ns_to_timer(unsigned int *ns, int round);

static const int nano_per_micro = 1000;	// 1000 nanosec in a microsec


static int start_interrupt( comedi_device *dev );

static int test_read(comedi_device * dev, comedi_subdevice * s,
                     comedi_insn * insn, lsampl_t * data);

static lsampl_t tmpval = 0;

static int mytest_open(comedi_device * dev)
{
    int result;
    printk("comedi%d: Opening device !!!\n", dev->minor);
    tmpval = 0;
    result = 0;
    return result;
}

static int mytest_close(comedi_device * dev)
{
    int result;
    printk("comedi%d: Closing device !!!\n", dev->minor);
    result = 0;
    return result;
}

int read_card( comedi_device * dev )
{
        int tmp = 0;
        printk("comedi%d: performing inb( 0x%x )\n",dev->minor , dev->iobase + 1  );
        tmp =  (0xFF & ~inb( dev->iobase + 0x1 ));
        printk("comedi%d: performing inb( 0x%x )\n",dev->minor , dev->iobase + 5 );
        tmp |= ( (0xFF & (~inb( dev->iobase + 0x5 ))) << 8 ); 
        return tmp;
}

static irqreturn_t accesisa_irq_handler(int irq, void *d PT_REGS_ARG)
{
  comedi_device *dev  = d;
  unsigned long flags;
  sampl_t tmp;
  comedi_subdevice *s = dev->read_subdev;
  printk("comedi%d: Handled interrupt: %d !\n", dev->minor, irq );
  comedi_spin_lock_irqsave(&dev->spinlock, flags);
  
  /* First get some data
     is our issue that lsampl_t is 32bit and we are only returning
     16 bit
  */
  
  tmp = (sampl_t)read_card( dev );
  /* /\* Believe it requires having a buffer first *\/ */
  /* printk("comedi%d: was going to write to buffer\n", dev->minor ); */
   /* cfc_write_long_to_buffer( dev->read_subdev, tmp ); */
  /* printk("comedi%d: Size is %d\n", dev->minor, dev->read_subdev->async->prealloc_bufsz ); */
  /* Clear interrupt */
  outb(0xF, dev->iobase + 0x1 );

  comedi_spin_unlock_irqrestore(&dev->spinlock, flags);

  printk("comedi%d: Sending event to subdevice %d\n", dev->minor, 2 );
  printk("Addres of subdev: %p\n", s );
  /* cfc_write_to_buffer( dev->read_subdev, tmp ); */
  /* s->async->events |= COMEDI_CB_EOS | COMEDI_CB_BLOCK ; /\* Indicate an end of scan *\/ */
  comedi_buf_put(s->async, tmp );
 
  /* Even this caused a failure */
  s->async->events |= COMEDI_CB_BLOCK | COMEDI_CB_EOS;
  /* printk("s->async: %p\n", s->async ); */
  /* s->async->events |= COMEDI_CB_EOS; /\* End of scan signal *\/ */
  comedi_event(dev, s);

  return IRQ_HANDLED;
}

/**
 * @todo Loop over the various types of boards, and set the appropriate
 *       subdevices accordingly.
 */
static int accesisa_attach(comedi_device * dev, comedi_devconfig * it)
{

	comedi_subdevice *s;
        int ret;
        /* Assign base address */
	printk("comedi%d: accesisa: ", dev->minor);

        /* Assign base address */
        
	dev->board_name = thisboard->name;
	printk("tried to assign name '%s' ", dev->board_name );

	if (alloc_private(dev, sizeof(accesisa_private)) < 0)
		return -ENOMEM;

        if ( it->options[0] < 0 ) { 
                return -ENOENT;
        }
        dev->iobase = it->options[0];
        if ( dev->iobase < 0 ) {
                printk("valid io base address required\n");
                return -EINVAL;
        }
        dev->irq = it->options[1];
        printk(" using base=0x%x ", dev->iobase );

        if ( !request_region( dev->iobase, thisboard->iosize, thisboard->name ) ) {
            printk("I/O port conflict: unable to allocate range 0x%lx to 0x%lx\n", dev->iobase, dev->iobase + thisboard->iosize-1 );
            return -EIO;
        }

        printk(" %s " , "Setting up Irq handler ");
        if (dev->irq == 0) {
          printk(" unknown irq (bad)\n");
        } else {
          if ((ret = comedi_request_irq(dev->irq, 
                                        accesisa_irq_handler,
                                        IRQF_SHARED, 
                                        "accesisa", 
                                        dev)) < 0) {
            printk(" Irq %d not available\n", dev->irq );
            dev->irq = 0;
          } else {
            printk(" Irq setup !!\n");
          }
        }
        /* printk(" %s ", "Setting up mytest_open"); */
        dev->open = mytest_open;
        dev->close = mytest_close;
        /*
         * Allocate the subdevice structures.  alloc_subdevice() is a
         * convenient macro defined in comedidev.h.
         */
	if (alloc_subdevices(dev, 2) < 0)
		return -ENOMEM;

        /*------------------------------  FIRST DEVICE -------------------------------*/
        /* Interupt */
	s = dev->subdevices + 0;
	dev->read_subdev = s;
        s->type = COMEDI_SUBD_DI;
	s->subdev_flags = SDF_READABLE | SDF_CMD_READ | SDF_GROUND ;
	/* s->n_chan = thisboard->num_out_bits; */
        s->n_chan = 8;
	s->len_chanlist = s->n_chan * 2; /* Caused bug */
        s->maxdata = 1;
	s->range_table = &range_digital;
        s->insn_bits    = accesisa_dio_insn_bits;
        s->insn_config  = accesisa_dio_insn_config;
	s->do_cmd       = waveform_ai_cmd;
	s->do_cmdtest   = waveform_ai_cmdtest;
        /* s->do_cmd       = accesisa_cmd_irq; */

        /*------------------------------  NEXT DEVICE  -------------------------------*/
	s = dev->subdevices + 1;
	/* Digital Output subdevice */
	s->type          = COMEDI_SUBD_DI;
	s->subdev_flags  = SDF_READABLE;
        s->range_table   = &range_digital;
	s->insn_write    = accesisa_ao_winsn;
	s->insn_read     = accesisa_ao_rinsn;
        s->insn_bits     = accesisa_dio_insn_bits;
        s->n_chan        = thisboard->num_in_bits;

	printk("attached\n");

	return 0;
}

static int start_interrupt( comedi_device *dev )
{
  int retval;
  printk("Starting the interrupt function\n");
  /* Disable IRQ */
  /* outb(0xF, dev->iobase + 0x2 ); */
#if 1
  outb(0xF, 0x302 );
  /* Clear IRQ */
  /* outb(0xF, dev->iobase + 0x1 ); */
  outb(0xF, 0x301 );
  /* Enable IRQ */
  /* inb(dev->iobase + 0x2 ); */
  inb(0x302 );
#endif
  /* s->async->events |= COMEDI_CB_EOS COMEDI_CB_BLOCK ; /\* Indicate an end of scan *\/ */
  return 0;
}

/*
 * _detach is called to deconfigure a device.  It should deallocate
 * resources.
 * This function is also called when _attach() fails, so it should be
 * careful not to release resources that were not necessarily
 * allocated by _attach().  dev->private and dev->subdevices are
 * deallocated automatically by the core.
 */
static int accesisa_detach(comedi_device * dev)
{
	printk("comedi%d: accesisa: remove\n", dev->minor);
        if ( dev->iobase ) {
            release_region( dev->iobase, thisboard->iosize );
        }

        printk("comedi%d: HERE!!: IRQ:%d\n", dev->minor, dev->irq );
        if (dev->irq ) {
          printk("comedi%d: removing the irq %d\n", dev->minor, dev->irq );
          comedi_free_irq(dev->irq, dev);
        }

	return 0;
}

/*
 * "instructions" read/write data in "one-shot" or "software-triggered"
 * mode.
 */
static int accesisa_ai_rinsn(comedi_device * dev, comedi_subdevice * s,
	comedi_insn * insn, lsampl_t * data)
{
        int n = 0;
	/* return the number of samples read/written */
	return n;
}

static int accesisa_cmd_irq(comedi_device * dev, comedi_subdevice * s)
{
  printk("comedi%d: Setting up irq\n", dev->minor );
  comedi_cmd *cmd = &s->async->cmd;
  unsigned long flags;
  int event = 0;
  /* comedi_spin_lock_irqsave(&subpriv->spinlock, flags); */
  /* subpriv->active = 1; */
  /* Set up end of acquisition. */
  /* Set up start of acquisition. */
#if 0
  switch (cmd->start_src) {
  case TRIG_INT:
    /* s->async->inttrig = dio200_inttrig_start_intr; /\*  *\/ */
    s->async->inttrig = start_interrupt;
    break;
  default:
    /* TRIG_NOW */
    /* event = dio200_start_intr(dev, s); */
    event = start_interrupt( dev );
    break;
  }
  if (event) {
    comedi_event(dev, s);
  }

#endif

  return 0;
}

static int waveform_ai_cmd(comedi_device * dev, comedi_subdevice * s)
{
	comedi_cmd *cmd = &s->async->cmd;
        int event;
	if (cmd->flags & TRIG_RT) {
		comedi_error(dev,
			"commands at RT priority not supported in this driver");
		return -1;
	}

        if ( cmd->start_src == TRIG_INT ) {
          printk("Doing some TRIG_INT\n");
        } else {
          printk("Starting the interrupt !\n");
          event = start_interrupt(dev );
          comedi_event( dev, s);
        }

	if (cmd->convert_src == TRIG_NOW)
		/* devpriv->convert_period = 0; */
          printk("comedi%d: Doing some TRIG_NOW stuff\n", dev->minor);
	else if (cmd->convert_src == TRIG_TIMER)
		/* devpriv->convert_period = cmd->convert_arg / nano_per_micro; */
          printk("comedi%d: Doing some TRIG_TIMER stuff\n",dev->minor);
	else {
          comedi_error(dev, "bug setting conversion period");
          return -1;
	}

	return 0;
}
#define MYDEBUG(str,counter,err) printk(str,dev->minor,(char)((counter++)+65),err);



static int waveform_ai_cmdtest(comedi_device * dev, comedi_subdevice * s,
	comedi_cmd * cmd)
{
	int err = 0;
	int tmp;
        int counter = 0;
	/* step 1: make sure trigger sources are trivially valid */
        printk("comedi%d: Calling cmd_test\n",dev->minor);
        printk("Start src is %d\n", cmd->start_src );
	tmp = cmd->start_src;
	cmd->start_src &= (TRIG_NOW | TRIG_INT);
	if (!cmd->start_src || tmp != cmd->start_src)
		err++;
        MYDEBUG("comedi%d: %c %d\n",counter,err);

	tmp = cmd->scan_begin_src;
	/* cmd->scan_begin_src &= ( TRIG_TIMER | TRIG_FOLLOW ); */
	cmd->scan_begin_src &= ( TRIG_TIMER | TRIG_FOLLOW );
	if (!cmd->scan_begin_src || tmp != cmd->scan_begin_src)
		err++;
        MYDEBUG("comedi%d: %c %d\n",counter,err);

	tmp = cmd->convert_src;
	/* cmd->convert_src &= TRIG_NOW | TRIG_TIMER; */
	cmd->convert_src &= TRIG_NOW;
	if (!cmd->convert_src || tmp != cmd->convert_src)
		err++;
        MYDEBUG("comedi%d: %c %d\n",counter,err);

	tmp = cmd->scan_end_src;
	cmd->scan_end_src &= TRIG_COUNT;
        printk("comedi_test: scan_end_src: %d\n", cmd->scan_end_src );
	if (!cmd->scan_end_src || tmp != cmd->scan_end_src)
		err++;

	tmp = cmd->stop_src;
        printk("comedi_test: stop_src: %d\n", cmd->stop_src );
	cmd->stop_src &= TRIG_NONE;
	if (!cmd->stop_src || tmp != cmd->stop_src)
		err++;

	if (err)
		return 1;

	/* step 2: make sure trigger sources are unique and mutually compatible */

	if (cmd->convert_src != TRIG_NOW && cmd->convert_src != TRIG_TIMER)
		err++;
	if (cmd->stop_src != TRIG_COUNT && cmd->stop_src != TRIG_NONE)
		err++;

	if (err)
		return 2;

	/* step 3: make sure arguments are trivially compatible */

	if (cmd->start_arg != 0) {
		cmd->start_arg = 0;
		err++;
	}

	if (cmd->convert_src == TRIG_NOW) {
		if (cmd->convert_arg != 0) {
			cmd->convert_arg = 0;
			err++;
		}
	}
	if (cmd->scan_begin_src == TRIG_TIMER) {
		if (cmd->scan_begin_arg < nano_per_micro) {
			cmd->scan_begin_arg = nano_per_micro;
			err++;
		}
		if (cmd->convert_src == TRIG_TIMER &&
			cmd->scan_begin_arg <
			cmd->convert_arg * cmd->chanlist_len) {
			cmd->scan_begin_arg =
				cmd->convert_arg * cmd->chanlist_len;
			err++;
		}
	}
	// XXX these checks are generic and should go in core if not there already
	if (!cmd->chanlist_len) {
		cmd->chanlist_len = 1;
		err++;
	}
	if (cmd->scan_end_arg != cmd->chanlist_len) {
		cmd->scan_end_arg = cmd->chanlist_len;
		err++;
	}

	if (cmd->stop_src == TRIG_COUNT) {
		if (!cmd->stop_arg) {
			cmd->stop_arg = 1;
			err++;
		}
	} else {		/* TRIG_NONE */
		if (cmd->stop_arg != 0) {
			cmd->stop_arg = 0;
			err++;
		}
	}

	if (err)
		return 3;

	/* step 4: fix up any arguments */

	if (cmd->scan_begin_src == TRIG_TIMER) {
		tmp = cmd->scan_begin_arg;
		// round to nearest microsec
		cmd->scan_begin_arg =
			nano_per_micro * ((tmp +
				(nano_per_micro / 2)) / nano_per_micro);
		if (tmp != cmd->scan_begin_arg)
			err++;
	}
	if (cmd->convert_src == TRIG_TIMER) {
		tmp = cmd->convert_arg;
		// round to nearest microsec
		cmd->convert_arg =
			nano_per_micro * ((tmp +
				(nano_per_micro / 2)) / nano_per_micro);
		if (tmp != cmd->convert_arg)
			err++;
	}

	if (err)
		return 4;

	return 0;
}

static int accesisa_ai_cmdtest(comedi_device * dev, comedi_subdevice * s,
	comedi_cmd * cmd)
{
	int err = 0;
	int tmp;

	/* cmdtest tests a particular command to see if it is valid.
	 * Using the cmdtest ioctl, a user can create a valid cmd
	 * and then have it executes by the cmd ioctl.
	 *
	 * cmdtest returns 1,2,3,4 or 0, depending on which tests
	 * the command passes. */

	/* step 1: make sure trigger sources are trivially valid */

	tmp = cmd->start_src;
	cmd->start_src &= TRIG_NOW;
	if (!cmd->start_src || tmp != cmd->start_src)
		err++;

	tmp = cmd->scan_begin_src;
	cmd->scan_begin_src &= TRIG_TIMER | TRIG_EXT;
	if (!cmd->scan_begin_src || tmp != cmd->scan_begin_src)
		err++;

	tmp = cmd->convert_src;
	cmd->convert_src &= TRIG_TIMER | TRIG_EXT;
	if (!cmd->convert_src || tmp != cmd->convert_src)
		err++;

	tmp = cmd->scan_end_src;
	cmd->scan_end_src &= TRIG_COUNT;
	if (!cmd->scan_end_src || tmp != cmd->scan_end_src)
		err++;

	tmp = cmd->stop_src;
	cmd->stop_src &= TRIG_COUNT | TRIG_NONE;
	if (!cmd->stop_src || tmp != cmd->stop_src)
		err++;

	if (err)
		return 1;

	/* step 2: make sure trigger sources are unique and mutually compatible */

	/* note that mutual compatiblity is not an issue here */
	if (cmd->scan_begin_src != TRIG_TIMER &&
		cmd->scan_begin_src != TRIG_EXT)
		err++;
	if (cmd->convert_src != TRIG_TIMER && cmd->convert_src != TRIG_EXT)
		err++;
	if (cmd->stop_src != TRIG_COUNT && cmd->stop_src != TRIG_NONE)
		err++;

	if (err)
		return 2;

	/* step 3: make sure arguments are trivially compatible */

	if (cmd->start_arg != 0) {
		cmd->start_arg = 0;
		err++;
	}
#define MAX_SPEED	10000	/* in nanoseconds */
#define MIN_SPEED	1000000000	/* in nanoseconds */

	if (cmd->scan_begin_src == TRIG_TIMER) {
		if (cmd->scan_begin_arg < MAX_SPEED) {
			cmd->scan_begin_arg = MAX_SPEED;
			err++;
		}
		if (cmd->scan_begin_arg > MIN_SPEED) {
			cmd->scan_begin_arg = MIN_SPEED;
			err++;
		}
	} else {
		/* external trigger */
		/* should be level/edge, hi/lo specification here */
		/* should specify multiple external triggers */
		if (cmd->scan_begin_arg > 9) {
			cmd->scan_begin_arg = 9;
			err++;
		}
	}
	if (cmd->convert_src == TRIG_TIMER) {
		if (cmd->convert_arg < MAX_SPEED) {
			cmd->convert_arg = MAX_SPEED;
			err++;
		}
		if (cmd->convert_arg > MIN_SPEED) {
			cmd->convert_arg = MIN_SPEED;
			err++;
		}
	} else {
		/* external trigger */
		/* see above */
		if (cmd->convert_arg > 9) {
			cmd->convert_arg = 9;
			err++;
		}
	}

	if (cmd->scan_end_arg != cmd->chanlist_len) {
		cmd->scan_end_arg = cmd->chanlist_len;
		err++;
	}
	if (cmd->stop_src == TRIG_COUNT) {
		if (cmd->stop_arg > 0x00ffffff) {
			cmd->stop_arg = 0x00ffffff;
			err++;
		}
	} else {
		/* TRIG_NONE */
		if (cmd->stop_arg != 0) {
			cmd->stop_arg = 0;
			err++;
		}
	}

	if (err)
		return 3;

	/* step 4: fix up any arguments */

	if (cmd->scan_begin_src == TRIG_TIMER) {
		tmp = cmd->scan_begin_arg;
		accesisa_ns_to_timer(&cmd->scan_begin_arg,
			cmd->flags & TRIG_ROUND_MASK);
		if (tmp != cmd->scan_begin_arg)
			err++;
	}
	if (cmd->convert_src == TRIG_TIMER) {
		tmp = cmd->convert_arg;
		accesisa_ns_to_timer(&cmd->convert_arg,
			cmd->flags & TRIG_ROUND_MASK);
		if (tmp != cmd->convert_arg)
			err++;
		if (cmd->scan_begin_src == TRIG_TIMER &&
			cmd->scan_begin_arg <
			cmd->convert_arg * cmd->scan_end_arg) {
			cmd->scan_begin_arg =
				cmd->convert_arg * cmd->scan_end_arg;
			err++;
		}
	}

	if (err)
		return 4;

	return 0;
}

/* This function doesn't require a particular form, this is just
 * what happens to be used in some of the drivers.  It should
 * convert ns nanoseconds to a counter value suitable for programming
 * the device.  Also, it should adjust ns so that it cooresponds to
 * the actual time that the device will use. */
static int accesisa_ns_to_timer(unsigned int *ns, int round)
{
	/* trivial timer */
	/* if your timing is done through two cascaded timers, the
	 * i8253_cascade_ns_to_timer() function in 8253.h can be
	 * very helpful.  There are also i8254_load() and i8254_mm_load()
	 * which can be used to load values into the ubiquitous 8254 counters
	 */

	return *ns;
}

static int accesisa_ao_winsn(comedi_device * dev, comedi_subdevice * s,
	comedi_insn * insn, lsampl_t * data)
{
	int i;
	int chan = CR_CHAN(insn->chanspec);

	printk("accesisa_ao_winsn\n");
	/* Writing a list of values to an AO channel is probably not
	 * very useful, but that's how the interface is defined. */
	/* for (i = 0; i < insn->n; i++) { */
	/* 	/\* a typical programming sequence *\/ */
	/* 	//outw(data[i],dev->iobase + ACCESISA_DA0 + chan); */
	/* 	devpriv->ao_readback[chan] = data[i]; */
	/* } */
	/* return the number of samples read/written */
	return i;
}

/* AO subdevices should have a read insn as well as a write insn.
 * Usually this means copying a value stored in devpriv. */
static int 
accesisa_ao_rinsn(comedi_device * dev, comedi_subdevice * s,
                  comedi_insn * insn, lsampl_t * data)
{
	int i;
	int chan = CR_CHAN(insn->chanspec);

	for (i = 0; i < insn->n; i++)
		data[i] = devpriv->ao_readback[chan];
        printk("comedi%d doing something in the accesisa_ao_rinsn\n", dev->minor );
        /* inb( BASE_ADDRESS ); */

	return i;
}

/**
 * @desc 
 *
 */
static int 
accesisa_dio_insn_config(comedi_device * dev, comedi_subdevice * s,
                         comedi_insn * insn, lsampl_t * data)
{
	int chan = CR_CHAN(insn->chanspec);
        printk("comedi%d: DIO configuring\n", dev->minor);
	/* The input or output configuration of each digital line is
	 * configured by a special insn_config instruction.  chanspec
	 * contains the channel to be changed, and data[0] contains the
	 * value COMEDI_INPUT or COMEDI_OUTPUT. */
	switch (data[0]) {
	case INSN_CONFIG_DIO_OUTPUT:
            printk("comedi%d: DIO Output\n", dev->minor);
            s->io_bits |= 1 << chan;
            break;
	case INSN_CONFIG_DIO_INPUT:
            printk("comedi%d: DIO Input\n", dev->minor);
            s->io_bits &= ~(1 << chan);
            break;
	case INSN_CONFIG_DIO_QUERY:
            printk("comedi%d: DIO Query\n", dev->minor);
            data[1] = ( s->io_bits & (1 << chan)) ? COMEDI_OUTPUT : COMEDI_INPUT;
            return insn->n;
            break;
	default:
		return -EINVAL;
		break;
	}

	return insn->n;
}


char *lookup_insn( unsigned int insn ) {
    switch(insn) {
    case INSN_READ:
        return "INSN_READ";
    case INSN_WRITE:
        return "INSN_WRITE";
    case INSN_BITS:
        return "INSN_BITS";
    case INSN_CONFIG:
        return "INSN_CONFIG";
    case INSN_GTOD:
        return "INSN_GTOD";
    case INSN_WAIT:
        return "INSN_WAIT";
    default:
        return "";
    }
}

void debug_comedi_insn( comedi_insn *insn )
{
    printk("INSN:\n");
    printk("  insn: %s\n", lookup_insn(insn->insn) );
    printk("  num: %d\n", insn->n );
}

/* DIO devices are slightly special.  Although it is possible to
 * implement the insn_read/insn_write interface, it is much more
 * useful to applications if you implement the insn_bits interface.
 * This allows packed reading/writing of the DIO channels.  The
 * comedi core can convert between insn_bits and insn_read/write */
static int accesisa_dio_insn_bits(comedi_device * dev, comedi_subdevice * s,
	comedi_insn * insn, lsampl_t * data)
{
        int i;
        debug_comedi_insn( insn );

        if (  insn->insn == INSN_READ ) {
            printk(	"comedi%d: Reading something\n", dev->minor);
        } else if ( insn->insn == INSN_WRITE ) {
            printk(	"comedi%d: Writing something\n", dev->minor);            
        } else if ( insn->insn == INSN_BITS ) {
            sampl_t temp;
            /* data[1] = tmpval; */
            printk(	"comedi%d: Bits something: %d, data=%d\n", dev->minor, (int)tmpval, (int)data[1] );            
            /**
             * @note Can i do a comedi_buf_get( ) here to grab
             *       data from the buffer....or instead, should I just 
             *       do the straight inb from the appropriate base address...
             */
            comedi_buf_get(s->async, &temp );
            data[0] = temp;
            /* ret = comedi_buf_get(s->async, &temp); */
            /* data[0] = temp; */
            /* data[1] = temp >> 8; */
        }

        printk("comedi%d: data[0]=%d\n", dev->minor, data[0] );
        printk("comedi%d: data[1]=%d\n", dev->minor, data[1] );
        /* Debug the instruction */
        if (insn->n != 2)       /* What does this actually mean here... */
		return -EINVAL;

        printk(	"comedi%d: Here\n", dev->minor );
        data[0] = 0;
        /* for ( i = 0 ; i < thisboard->num_read_bases ; i ++ ) { */
        /*     printk("comedi%d: performing inb( 0x%x )\n", dev->minor, dev->iobase + thisboard->read_bases[i] ); */
        /*     data[0] = tmpval; */
        /* } */

        /* Checking something...is this necessary */
        /* You need to replace this part ... */
        if (data[0]) {
                /* Check if requested ports are configured for output */
                /* if ((s->io_bits & data[0]) != data[0]) */
                /*         return -EIO; */
                /* s->state &= ~data[0]; */
                s->state |= data[0] & data[1];
        }

	return 2;
}

/* data[0] = 0; */
/* s->state = data[1]; */

/* The insn data is a mask in data[0] and the new data
 * in data[1], each channel cooresponding to a bit. */
/* data[0] = inb( dev->iobase + correct_offset ); */
/* data[1] = inb( dev->iobase + correct_second_offset ); */
/* on return, data[1] contains the value of the digital
 * input and output lines. */
//data[1]=inw(dev->iobase + ACCESISA_DIO);
/* or we could just return the software copy of the output values if
 * it was a purely digital output subdevice */
//data[1]=s->state;


/*
 * A convenient macro that defines init_module() and cleanup_module(),
 * as necessary.
 */
COMEDI_INITCLEANUP(driver_accesisa);
/* If you are writing a PCI driver you should use COMEDI_PCI_INITCLEANUP instead.
*/
// COMEDI_PCI_INITCLEANUP(driver_accesisa, accesisa_pci_table)

/* GARBAGE */

/* /\** */
/*  * @desc  */
/*  * */
/*  *\/ */
/* static int skel_dio_insn_config(comedi_device * dev, comedi_subdevice * s, */
/* 	comedi_insn * insn, lsampl_t * data) */
/* { */
/* 	int chan = CR_CHAN(insn->chanspec); */
/* 	/\* The input or output configuration of each digital line is */
/* 	 * configured by a special insn_config instruction.  chanspec */
/* 	 * contains the channel to be changed, and data[0] contains the */
/* 	 * value COMEDI_INPUT or COMEDI_OUTPUT. *\/ */
/* 	switch (data[0]) { */
/* 	case INSN_CONFIG_DIO_OUTPUT: */
/* 		s->io_bits |= 1 << chan; */
/* 		break; */
/* 	case INSN_CONFIG_DIO_INPUT: */
/* 		s->io_bits &= ~(1 << chan); */
/* 		break; */
/* 	case INSN_CONFIG_DIO_QUERY: */
/* 		data[1] = ( s->io_bits & (1 << chan)) ? COMEDI_OUTPUT : COMEDI_INPUT; */
/* 		return insn->n; */
/* 		break; */
/* 	default: */
/* 		return -EINVAL; */
/* 		break; */
/* 	} */
/* 	//outw(s->io_bits,dev->iobase + SKEL_DIO_CONFIG); */
/* 	return insn->n; */
/* } */
