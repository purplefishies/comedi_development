

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
    PORT_C = 2
} PORT;

typedef struct {
    

} io_region_t;


typedef struct {
    struct pci_dev *pdev;
    int data;
    struct pci_dev *pci_dev;
    lsampl_t ao_readback[2];
    unsigned char mode[5];
    void *dio_base;
    int got_regions;
    int plx_region_start;
    int plx_region_length;
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

static int apci_dio_read_cmd(comedi_device * dev, comedi_subdevice * s );
static int apci_dio_read_cmdtest(comedi_device * dev, comedi_subdevice * s,
        comedi_cmd * cmd);

char debug_port( PORT port )
{
    char retval;
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
        retval = '_';
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

static int apci_attach(comedi_device * dev, comedi_devconfig * it)
{
	comedi_subdevice *s;
        subdev_private *spriv;
        struct pci_dev *pdev;
        int index;
        resource_size_t resourceStart;
        int plx_bar;

	apci_info("comedi%d: \n", dev->minor);

	//dev->board_ptr = apci_probe(dev, it);

	if (alloc_private(dev, sizeof(apci_private)) < 0)
 		return -ENOMEM;

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


        /* devpriv->plx_region_end        = pci_resource_end(pdev, plx_bar); */
        /* if( ! devpriv->plx_region_end ) { */
        /*     apci_error("Invalid bar %d on end", plx_bar ); */
        /*     return -ENODEV; */
        /* } */


        /* if (comedi_pci_enable(pdev, thisboard->name)) { */
        /*     apci_error("failed to enable PCI device and request regions\n"); */
        /*     return -EIO; */
        /* } */
        /* devpriv->got_regions = 1; */

        /* resourceStart = pci_resource_start(pdev, 2 ); */

        /* devpriv->dio_base = pci_resource_start(devpriv->pdev, (pci_resource_len(devpriv->pdev, 2) ? 2 : 1)); */
        /* devpriv->dio_base = ioremap(resourceStart, SIZEOF_ADDRESS_SPACE); */
        /* devpriv->dio_base =  */

        /* if (devpriv->dio_base == NULL) { */
        /*         apci_error("apci_attach: IOREMAP failed\n"); */
        /*         return -ENODEV; */
        /* } else { */
        /*     apci_debug("Got address of %08x\n", (int)devpriv->dio_base ); */
        /* } */


        /* Setup private stuff */
        apci_debug("Size of devpriv->mode: %d\n", (int)(sizeof(devpriv->mode)) );
        for ( int i = 0; i < sizeof(devpriv->mode) / sizeof(unsigned char ); i ++ ) {
            devpriv->mode[i] = MAKE_BYTE( MODE_ACTIVE_AND_TRISTATE, MODE0_HIGH, MODE0_LOW, INPUT,INPUT,MODE_SEL_MODE0,INPUT,INPUT);
            apci_debug("mode[%d]: %s\n", i, debug_byte( devpriv->mode[i] ) );
        } 

        /* if ((result = comedi_pci_enable(pdev, "s626")) < 0) { */
        /*     printk("s626_attach: comedi_pci_enable fails\n"); */
        /*     return -ENODEV; */
        /* } */
        /* resourceStart = pci_resource_start(devpriv->pdev, 0); */
        /* devpriv->base_addr = ioremap(resourceStart, SIZEOF_ADDRESS_SPACE); */

	if (alloc_subdevices(dev, 4) < 0)
		return -ENOMEM;

        /*--------------------------------  A  --------------------------------*/
	s = dev->subdevices + 0;
	
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
        spriv->group = 0;
        s->private = spriv;

        /*--------------------------------  B  --------------------------------*/
	s = dev->subdevices + 1;
	
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
        spriv->group = 0;
        s->private = spriv;
        
        /*--------------------------------  Chi  --------------------------------*/
	s = dev->subdevices + 2;

	s->type = COMEDI_SUBD_DIO;
	s->subdev_flags = SDF_READABLE | SDF_WRITABLE | SDF_CMD_READ;
	s->n_chan = 4;
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
        spriv->group = 0;
        s->private = spriv;

        /*--------------------------------  Clo  --------------------------------*/
	s = dev->subdevices + 3;

	s->type = COMEDI_SUBD_DIO;
	s->subdev_flags = SDF_READABLE | SDF_WRITABLE | SDF_CMD_READ;
	s->n_chan = 4;
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
        spriv->group = 0;
        s->private = spriv;

        s->do_cmd      = apci_dio_read_cmd;
        s->do_cmdtest  = apci_dio_read_cmdtest;

        /* Done */
	apci_debug("attached\n");

	return 0;
}

static int apci_dio_read_cmd(comedi_device * dev, comedi_subdevice * s)
{
    return 0;
}

/**
 * 
 * @todo Activate the IRQs
 * @todo Deactivate the IRQs
 * 
 */
static int apci_dio_read_cmdtest(comedi_device * dev, comedi_subdevice * s,
        comedi_cmd * cmd)
{
    int tmp;
    int err = 0;
    tmp = cmd->start_src;
    cmd->start_src &= TRIG_NOW | TRIG_INT ;
    if (!cmd->start_src || tmp != cmd->start_src)
        err++;

    tmp = cmd->scan_begin_src;
    cmd->scan_begin_src &= TRIG_FOLLOW;
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

    if (err) {
        return 1;
    }


    return 0;
}

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

        if (devpriv->dio_base ) {
            iounmap( devpriv->dio_base );
        }

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

static int apci_dio_insn_bits(comedi_device * dev, comedi_subdevice * s,
	comedi_insn * insn, lsampl_t * data)
{

        apci_debug("Writing/Reading\n");
	if (insn->n != 2)
		return -EINVAL;

	
	if (data[0]) {
		s->state &= ~data[0];
		s->state |= data[0] & data[1];
		//outw(s->state,dev->iobase + APCI_DIO);
	}

	
	//data[1]=inw(dev->iobase + APCI_DIO);
	
	//data[1]=s->state;

	return 2;
}

static int apci_dio_insn_config(comedi_device * dev, comedi_subdevice * s,
	comedi_insn * insn, lsampl_t * data)
{
	int chan = CR_CHAN(insn->chanspec);
        int is_output = 0;
        subdev_private *spriv;
        spriv = s->private;
        apci_debug("Configuring...using channels: %d\n", chan );
	
	switch (data[0]) {
	case INSN_CONFIG_DIO_OUTPUT:
            apci_debug("Setting all channels of Group %d Port %c to output\n", spriv->group,debug_port( spriv->port) );
            s->io_bits = 0xFF;
            is_output = 1;
            break;
	case INSN_CONFIG_DIO_INPUT:
            apci_debug("Setting all channels of Group %d Port %c to input\n",  spriv->group, debug_port( spriv->port ) );
            s->io_bits = 0x00;
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

        /* comedi_spin_lock_irqsave(&devpriv->lock, flags); */
        /* outb( devpriv->base + spriv->group*4 + 1, ) */
        /* comedi_spin_unlock_irqrestore(&devpriv->lock, flags); */
        /* MAKE_BYTE( )  */
        /* outb( s->base + 3, dev->iobase + s->group  ); */
        /* apci_debug("Writing out the new thing\n"); */
	//outw(s->io_bits,dev->iobase + APCI_DIO_CONFIG);

	return insn->n;
}


/* COMEDI_INITCLEANUP(driver_apci); */
COMEDI_PCI_INITCLEANUP(driver_apci, apci_pci_table)

