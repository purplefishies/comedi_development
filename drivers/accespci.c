

#include <linux/comedidev.h>
#include <linux/pci.h>		
#include "apci_common.h"
#include "comedi_pci.h"


#define APCI_SIZE 0

#define APCI_START_AI_CONV	0
#define APCI_AI_READ		0


#define INPUT 1
#define OUTPUT 0
#define MODE_SEL_MODE1 1
#define MODE_SEL_MODE0 0

#define MODE_ACTIVE_AND_TRISTATE 1
#define SIZEOF_ADDRESS_SPACE  0x20
 
#define MODE1_HIGH 1
#define MODE1_LOW  0
#define MODE0_HIGH 0
#define MODE0_LOW  0

#define MAKE_BYTE(A,B,C,D,E,F,G,H) ( ( (A&1) << 7 ) | ( (B&1) << 6 ) | ( (C&1) << 5 ) | ( (D&1) << 4 ) | \
                                     ( (E&1) << 3 ) | ( (F&1) << 2 ) | ( (G&1) << 1 ) | ( (H&1) ) )


typedef struct apci_board_struct {
    const char *name;
    int ai_chans;
    int ai_bits;
    int have_dio;
    int dev_id;
} apci_board;

typedef enum {
    PORT_A = 0,
    PORT_B = 1,
    PORT_C = 2,
    PORT_CHI = 2,
    PORT_CLO = 3
} PORT;

typedef struct {
    int start;
    int length;
    int flags;
    int enabled;
    void *mapped_address;
} io_region_t;


typedef struct {
    struct pci_dev *pdev;
    int data;
    struct pci_dev *pci_dev;
    lsampl_t ao_readback[2];
    unsigned char mode[5];
    void *dio_base;
    int got_regions;
    spinlock_t lock;
    int plx_region_start;
    int plx_region_length;
    io_region_t regions[6];
} apci_private;

typedef struct {
    int offset;
    PORT port;
    int group;

} subdev_private;


#define PCI_VENDOR_ID_APCI 0x494f
#define PCI_DIO_24 0x0c50
#define PCI_DIO_48 0x0c60

static const apci_board apci_boards[] = {
    {
    name:	"pci_dio_24",
    dev_id:  PCI_DIO_24 ,
    ai_chans:16,
    ai_bits:	12,
    have_dio:1,
    },
    {
    name:	"pci_dio_48",
    dev_id: PCI_DIO_48,
    ai_chans:8,
    ai_bits:	16,
    have_dio:0,
    },
};



static DEFINE_PCI_DEVICE_TABLE(apci_pci_table) = {
	{PCI_VENDOR_ID_APCI, PCI_DIO_24, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_APCI, PCI_DIO_48, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{0}
};

MODULE_DEVICE_TABLE(pci, apci_pci_table);

#define thisboard ((const apci_board *)dev->board_ptr)



#define devpriv ((apci_private *)dev->private)


static int apci_attach(comedi_device * dev, comedi_devconfig * it);
static int apci_detach(comedi_device * dev);
static comedi_driver driver_apci = {
      driver_name:"dummy",
      module:THIS_MODULE,
      attach:apci_attach,
      detach:apci_detach,
      board_name:&apci_boards[0].name,
      offset:sizeof(apci_board),
      num_names:sizeof(apci_boards) / sizeof(apci_board),
};


static int apci_dio_insn_bits(comedi_device * dev, comedi_subdevice * s,
	comedi_insn * insn, lsampl_t * data);
static int apci_dio_insn_config(comedi_device * dev, comedi_subdevice * s,
	comedi_insn * insn, lsampl_t * data);

/* static int apci_dio_read_cmd(comedi_device * dev, comedi_subdevice * s ); */
/* static int apci_dio_read_cmdtest(comedi_device * dev, comedi_subdevice * s, */
/*         comedi_cmd * cmd); */

char debug_port( PORT port )
{
    static char retval;
    switch (port) {
    case PORT_A:
        retval = 'A';
        break;
    case PORT_B:
        retval = 'B';
        break;
    case PORT_C:
        retval = 'C';
        break;
    default:
        retval =  '_' ;
        break;
    }
    return retval;
}

char *debug_byte( unsigned char byte ) 
{
    static char buf[9];
    for( int i = 7 ; i >= 0 ; i -- ) { 
        /* apci_debug("%d", (int)( byte & (1 << i ))); */
        buf[7-i] = ( byte & ( 1 << i) ? '1' : '0' );
    }
    buf[8] = 0;
    return buf;
}


void cleanup_io_regions( comedi_device *dev )
{

    for (int i = 0; i < 6; i ++) {
        if (devpriv->regions[i].start == 0) {
            continue; /* invalid region */
        }
        if (devpriv->regions[i].flags & IORESOURCE_IO) {
            release_region(devpriv->regions[i].start, devpriv->regions[i].length);
            apci_debug("Releasing bar=%d,region=%#x,len=%#x\n", i, devpriv->regions[i].start, devpriv->regions[i].length );
        } else {
            iounmap(devpriv->regions[i].mapped_address);
            apci_debug("Unmapping bar=%d,region=%#x,mapped=%lx\n", i, devpriv->regions[i].start, (long int)devpriv->regions[i].mapped_address );
            release_mem_region(devpriv->regions[i].start, devpriv->regions[i].length);
            apci_debug("Releasing mem bar=%d,region=%#x,len=%#x\n", i, devpriv->regions[i].start, devpriv->regions[i].length );
        }
    }
}


static int apci_attach(comedi_device * dev, comedi_devconfig * it)
{
	comedi_subdevice *s;
        subdev_private *spriv;
        struct pci_dev *pdev;
        int index;
        int plx_bar;

        struct resource *presource;

	apci_info("comedi%d: \n", dev->minor);

	//dev->board_ptr = apci_probe(dev, it);

	if (alloc_private(dev, sizeof(apci_private)) < 0)
 		return -ENOMEM;

        apci_debug("Initial: Start=%#x, length=%#x\n", devpriv->plx_region_start, devpriv->plx_region_length );        

        for (pdev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, NULL);
                pdev != NULL;
                pdev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, pdev)) {
                // is it not a computer boards card?
                /* apci_debug("looping: %x %x\n", pdev->vendor, pdev->device ); */
                if (pdev->vendor != PCI_VENDOR_ID_APCI)
                        continue;
                // loop through cards supported by this driver
                for (index = 0;
                        index < sizeof apci_boards / sizeof(apci_board);
                        index++) {
                        if (apci_boards[index].dev_id != pdev->device)
                                continue;

                        // was a particular bus/slot requested?
                        if (it->options[0] || it->options[1]) {
                                // are we on the wrong bus/slot?
                                if (pdev->bus->number != it->options[0] ||
                                        PCI_SLOT(pdev->devfn) !=
                                        it->options[1]) {
                                        continue;
                                }
                        }
                        dev->board_ptr = apci_boards + index;
                        goto found;
                }
        }
        printk("No supported ACCES I/O boards found\n");
        return -EIO;
        
 found:

        /* if (pdev == NULL) { */
        /*     apci_error("apci_attach: Board not present!!!\n"); */
        /*     return -ENODEV; */
        /* } */
        dev->board_name = thisboard->name;

        spin_lock_init(&devpriv->lock);

        apci_info("Found a device: %s\n", thisboard->name );

        devpriv->pdev = pdev;

        if (pci_resource_flags(pdev, 0) & IORESOURCE_IO) {
            plx_bar = 0;
        } else {
            plx_bar = 1;
        }

        apci_debug("Using bar=%d\n",plx_bar );
        devpriv->plx_region_start      = pci_resource_start(pdev, plx_bar);
        if( ! devpriv->plx_region_start ) {
            apci_error("Invalid bar %d on start ", plx_bar );
            return -ENODEV;
        }
        devpriv->plx_region_length     = pci_resource_len(pdev, plx_bar );
        apci_debug("Start=%#x, length=%#x\n", devpriv->plx_region_start, devpriv->plx_region_length );        

        /* Now request the region and also clean it up */
        presource = request_region(devpriv->plx_region_start, devpriv->plx_region_length, "accespci");
        if ( !presource ) {
            printk("I/O port conflict\n");
            return -EIO;
        }

        devpriv->regions[2].start   = pci_resource_start(pdev, 2);
        devpriv->regions[2].length  = pci_resource_len(pdev, 2);
        devpriv->regions[2].flags   = pci_resource_flags(pdev, 2);


        apci_debug("bar[%d], start=%#x,len=%#x\n", 2,devpriv->regions[2].start, devpriv->regions[2].length );
        if (devpriv->regions[2].flags & IORESOURCE_IO) {
            apci_debug("requesting io region start=%08x,len=%d\n", devpriv->regions[2].start, devpriv->regions[2].length );
            presource = request_region(devpriv->regions[2].start, devpriv->regions[2].length, "accespci");
        } else {
            apci_debug("requesting mem region start=%08x,len=%d\n", devpriv->regions[2].start, devpriv->regions[2].length );
            presource = request_mem_region(devpriv->regions[2].start, devpriv->regions[2].length, "accespci");
            if (presource != NULL) {
                apci_debug("Remapping address start=%08x,len=%d\n", devpriv->regions[2].start, devpriv->regions[2].length );
                devpriv->regions[2].mapped_address = ioremap(devpriv->regions[2].start, devpriv->regions[2].length);
            }
        }
        if ( !presource ) {
            apci_error("I/O port conflict\n");
            cleanup_io_regions( dev );
            return -EIO;
        } else {
            devpriv->regions[2].enabled = 1;
        }
        /* Don't need the prior if we can use the comedi_pci_enable */
        /* if ((result = comedi_pci_enable(pdev, "accespci")) < 0) { */
        /*     printk("apci_attach: comedi_pci_enable fails\n"); */
        /*     cleanup_io_regions( dev ); */
        /*     return -ENODEV; */
        /* } */

        /* Setup private stuff */
        apci_debug("Size of devpriv->mode: %d\n", (int)(sizeof(devpriv->mode)) );
        for ( int i = 0; i < sizeof(devpriv->mode) / sizeof(unsigned char ); i ++ ) {
            devpriv->mode[i] = MAKE_BYTE( MODE_ACTIVE_AND_TRISTATE, MODE0_HIGH, MODE0_LOW, INPUT,INPUT,MODE_SEL_MODE0,INPUT,INPUT);
            apci_debug("mode[%d]: %s\n", i, debug_byte( devpriv->mode[i] ) );
        } 

        
	if (alloc_subdevices(dev, 6) < 0)
		return -ENOMEM;

        for ( int group = 0; group < 2 ; group ++ ) { 

            /*--------------------------------  A  --------------------------------*/
            s = dev->subdevices + 0 + (group*3);
	
            s->type          = COMEDI_SUBD_DIO;
            s->subdev_flags  = SDF_READABLE | SDF_WRITABLE;
            s->n_chan        = 8;
            s->maxdata       = 1;
            s->range_table   = &range_digital;
            s->insn_bits     = apci_dio_insn_bits;
            s->insn_config   = apci_dio_insn_config;
            spriv = (subdev_private*)kmalloc(sizeof(subdev_private), GFP_KERNEL );
            if (!spriv) {
                apci_error("comedi%d: error! out of memory!\n", dev->minor);
                return -ENOMEM;
            }
            spriv->port  = PORT_A;
            spriv->group = group;
            s->private = spriv;

            /*--------------------------------  B  --------------------------------*/
            s = dev->subdevices + 1 + (group*3);
	
            s->type          = COMEDI_SUBD_DIO;
            s->subdev_flags  = SDF_READABLE | SDF_WRITABLE;
            s->n_chan        = 8;
            s->maxdata       = 1;
            s->range_table   = &range_digital;
            s->insn_bits     = apci_dio_insn_bits;
            s->insn_config   = apci_dio_insn_config;
            spriv = (subdev_private*)kmalloc(sizeof(subdev_private), GFP_KERNEL );
            if (!spriv) {
                apci_error("comedi%d: error! out of memory!\n", dev->minor);
                return -ENOMEM;
            }
            spriv->port  = PORT_B;
            spriv->group = group;
            s->private = spriv;
        
            /*--------------------------------  CHI & CLO-------------------------------*/
            s = dev->subdevices + 2 + (group*3) ;

            s->type = COMEDI_SUBD_DIO;
            s->subdev_flags = SDF_READABLE | SDF_WRITABLE | SDF_CMD_READ;
            s->n_chan = 8;
            s->maxdata = 1;
            s->range_table = &range_digital;
            s->insn_bits = apci_dio_insn_bits;
            s->insn_config = apci_dio_insn_config;	
            spriv = (subdev_private*)kmalloc(sizeof(subdev_private), GFP_KERNEL );
            if (!spriv) {
                apci_error("comedi%d: error! out of memory!\n", dev->minor);
                return -ENOMEM;
            }
            spriv->port  = PORT_C;
            spriv->group = group;
            s->private = spriv;
        }

        /* Done */
	apci_debug("attached\n");

	return 0;
}

/* static int apci_dio_read_cmd(comedi_device * dev, comedi_subdevice * s) */
/* { */
/*     return 0; */
/* } */

/**
 * 
 * @todo Activate the IRQs
 * @todo Deactivate the IRQs
 * 
 */
/* static int apci_dio_read_cmdtest(comedi_device * dev, comedi_subdevice * s, */
/*         comedi_cmd * cmd) */
/* { */
/*     int tmp; */
/*     int err = 0; */
/*     tmp = cmd->start_src; */
/*     cmd->start_src &= TRIG_NOW | TRIG_INT ; */
/*     if (!cmd->start_src || tmp != cmd->start_src) */
/*         err++; */

/*     tmp = cmd->scan_begin_src; */
/*     cmd->scan_begin_src &= TRIG_FOLLOW; */
/*     if (!cmd->scan_begin_src || tmp != cmd->scan_begin_src) */
/*         err++; */

/*     tmp = cmd->convert_src; */
/*     cmd->convert_src &= TRIG_TIMER | TRIG_EXT; */
/*     if (!cmd->convert_src || tmp != cmd->convert_src) */
/*         err++; */

/*     tmp = cmd->scan_end_src; */
/*     cmd->scan_end_src &= TRIG_COUNT; */
/*     if (!cmd->scan_end_src || tmp != cmd->scan_end_src) */
/*         err++; */

/*     tmp = cmd->stop_src; */
/*     cmd->stop_src &= TRIG_COUNT | TRIG_NONE; */
/*     if (!cmd->stop_src || tmp != cmd->stop_src) */
/*         err++; */

/*     if (err) { */
/*         return 1; */
/*     } */


/*     return 0; */
/* } */

static int apci_detach(comedi_device * dev)
{
        if (devpriv) {
            if (devpriv->pdev) {
                if (devpriv->got_regions) {
                    comedi_pci_disable(devpriv->pci_dev);
                }
                pci_dev_put(devpriv->pdev);
            }
        }

        if (devpriv->plx_region_start ) {
            apci_debug("Releasing plx_region_start=%#x,len=%#x\n", devpriv->plx_region_start, devpriv->plx_region_length );
            release_region( devpriv->plx_region_start, devpriv->plx_region_length);
        }

        cleanup_io_regions( dev );

        if (dev->subdevices) {
            for (int n = 0; n < dev->n_subdevices; n++) {
                comedi_subdevice *s = &dev->subdevices[n];
                subdev_private *spriv = s->private;
                if (spriv) {
                    apci_info("freeing private for subdevice %d\n", n);
                    kfree(spriv);
                }
            }
        }
        apci_debug("Detached\n");
	return 0;
}

int get_port_value( PORT port )
{
    switch (port ) {
    case PORT_A:
        return PORT_A;
    case PORT_B:
        return PORT_B;
    case PORT_C:
        return PORT_C;
    default:
        return PORT_A;
    }
}

static int apci_dio_insn_bits(comedi_device * dev, comedi_subdevice * s,
	comedi_insn * insn, lsampl_t * data)
{
        subdev_private *spriv = s->private;

        apci_debug("Writing/Reading\n");
	if (insn->n != 2)
		return -EINVAL;
	
	if (data[0]) {

            apci_debug("io_bits: %s\n",debug_byte( s->io_bits ));
            if ( (s->io_bits & data[0]) != data[0] ) 
                return -EIO;

            s->state &= ~data[0];
            s->state |= data[0] & data[1];

            apci_debug("writing '%s' to %#x\n", debug_byte(s->state), devpriv->regions[2].start + spriv->group*4 + get_port_value(spriv->port) );
            outb( s->state, devpriv->regions[2].start + spriv->group*4 + get_port_value(spriv->port) );
	}
        apci_debug("reading from %#x\n", devpriv->regions[2].start + spriv->group*4 + get_port_value(spriv->port) );
        data[1] = inb( devpriv->regions[2].start + spriv->group*4 + get_port_value(spriv->port) );
        apci_debug("read '%s'\n", debug_byte(data[1]) );

	return 2;
}

unsigned char calculate_direction( subdev_private *spriv, unsigned char omode, unsigned char bits, int channel )
{
    unsigned char retval;
    unsigned char incval;
    unsigned char exclval;
    int invbits = ( bits == 0 ? 1 : 0 );
    /* apci_debug("bits was %s: port %c\n", debug_byte(bits), debug_port( spriv->port )); */
    switch ( spriv->port ) {
    case PORT_A:
        incval   = MAKE_BYTE(0,0,0,invbits,0,0,0,0);
        exclval  = MAKE_BYTE(1,1,1,0,1,1,1,1);
        break;
    case PORT_B:
        incval   = MAKE_BYTE(0,0,0,0,0,0,invbits,0);
        exclval  = MAKE_BYTE(1,1,1,1,1,1,0,1);
        break;
    case PORT_C:
        if ( channel <= 3 ) {
            /* apci_debug("Using <= 3, channel=%d, bits=%d\n",channel, bits); */
            incval   = MAKE_BYTE(0,0,0,0,0,0,0,(bits & 0xf)==0?1:0);
            exclval  = MAKE_BYTE(1,1,1,1,1,1,1,0);
        } else {
            /* apci_debug("Using >= 4, channel=%d, bits=%d\n",channel,bits ); */
            incval   = MAKE_BYTE(0,0,0,0,(bits&0xf0)==0?1:0,0,0,0);
            exclval  = MAKE_BYTE(1,1,1,1,0,1,1,1);
        }
        break;
    default:
        incval = 0xff;
    }
    retval = (exclval & omode) | ( incval ); 

    return retval;
}

int make_io_bits( comedi_subdevice *s , int channel, int is_output ) 
{
    subdev_private *spriv = s->private;
    switch ( spriv->port ) {
    case PORT_A:
        if ( is_output ) 
            return 0xff;
        else
            return 0;
        break;
    case PORT_B:
        if ( is_output ) 
            return 0xff;
        else 
            return 0;
        break;
    case PORT_C:
        if ( is_output ) {
            if ( channel <= 3 ) {
                s->io_bits |= 0x0f;
            } else {
                s->io_bits |= 0xf0;
            }
        } else {
            if ( channel <= 3 ) { 
                s->io_bits &= 0xf0;
            } else {
                s->io_bits &= 0x0f;
            }
        }
        break;
    default:
        break;
    }
    apci_debug("make_io_bits: returning %s\n", debug_byte( s->io_bits ));
    return s->io_bits;
}

static int apci_dio_insn_config(comedi_device * dev, comedi_subdevice * s,
	comedi_insn * insn, lsampl_t * data)
{
	int chan = CR_CHAN(insn->chanspec);
        int is_output = 0;
        unsigned long flags;
        subdev_private *spriv;
        spriv = s->private;
        apci_debug("Configuring...using channel: %d\n", chan );
	
	switch (data[0]) {
	case INSN_CONFIG_DIO_OUTPUT:
            apci_debug("Setting all channels of Group %d Port %c to output ", spriv->group,debug_port( spriv->port) );
            s->io_bits = make_io_bits( s, chan, 1 );
            apci_debug("%s\n",debug_byte(s->io_bits));
            is_output = 1;
            break;
	case INSN_CONFIG_DIO_INPUT:
            apci_debug("Setting all channels of Group %d Port %c to input ",  spriv->group, debug_port( spriv->port ) );
            s->io_bits = make_io_bits( s, chan, 0 );
            apci_debug("%s\n",debug_byte(s->io_bits));
            is_output = 0;
            break;
	case INSN_CONFIG_DIO_QUERY:
		data[1] = (s-> io_bits & (1 << chan)) ? COMEDI_OUTPUT : COMEDI_INPUT;
		return insn->n;
		break;
	default:
		return -EINVAL;
		break;
	}

        spriv = s->private;
        if ( spriv ) {
            apci_debug("Sending signal to Base + %d\n", (spriv->port + spriv->group*4 ));
        }

        comedi_spin_lock_irqsave(&devpriv->lock, flags);


        apci_debug("Before: %s\n", debug_byte(devpriv->mode[spriv->group]) );
        /* devpriv->mode[spriv->group] = devpriv->mode[spriv->group] & calculate_direction( spriv, ~s->io_bits ); */
        devpriv->mode[spriv->group] = calculate_direction( spriv, devpriv->mode[spriv->group], s->io_bits , chan  );
        apci_debug("After:  %s\n", debug_byte(devpriv->mode[spriv->group]) );
        apci_debug("Sending write to + %#x\n", devpriv->regions[2].start + spriv->group*4 + 3 );
        outb(devpriv->mode[spriv->group] , devpriv->regions[2].start + spriv->group*4 + 3 );
        apci_debug("Read: %s\n", debug_byte( inb(devpriv->regions[2].start + spriv->group*4 + 3 )));

        comedi_spin_unlock_irqrestore(&devpriv->lock, flags);

	return insn->n;
}



COMEDI_PCI_INITCLEANUP(driver_apci, apci_pci_table)

