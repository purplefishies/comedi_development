

#include <linux/comedidev.h>
#include "apci_common.h"

#include <linux/pci.h>		



#define SKEL_SIZE 0

#define SKEL_START_AI_CONV	0
#define SKEL_AI_READ		0


typedef struct apci_board_struct {
	const char *name;
	int ai_chans;
	int ai_bits;
	int have_dio;
} apci_board;
static const apci_board apci_boards[] = {
    {
    name:	"pci_dio_24",
    ai_chans:16,
    ai_bits:	12,
    have_dio:1,
    },
    {
    name:	"pci_dio_48",
    ai_chans:8,
    ai_bits:	16,
    have_dio:0,
    },
};



#define PCI_VENDOR_ID_SKEL 0xdafe
static DEFINE_PCI_DEVICE_TABLE(apci_pci_table) = {
	{PCI_VENDOR_ID_SKEL, 0x0100, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_SKEL, 0x0200, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{0}
};

MODULE_DEVICE_TABLE(pci, apci_pci_table);


#define thisboard ((const apci_board *)dev->board_ptr)


typedef struct {
	int data;

	
	struct pci_dev *pci_dev;

	
	lsampl_t ao_readback[2];
} apci_private;

#define devpriv ((apci_private *)dev->private)


static int apci_attach(comedi_device * dev, comedi_devconfig * it);
static int apci_detach(comedi_device * dev);
static comedi_driver driver_skel = {
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

static int apci_attach(comedi_device * dev, comedi_devconfig * it)
{
	comedi_subdevice *s;

	apci_info("comedi%d: \n", dev->minor);

	//dev->board_ptr = apci_probe(dev, it);

	dev->board_name = thisboard->name;


	if (alloc_private(dev, sizeof(apci_private)) < 0)
		return -ENOMEM;


	if (alloc_subdevices(dev, 4) < 0)
		return -ENOMEM;

        /*--------------------------------  A  --------------------------------*/
	s = dev->subdevices + 0;
	
	s->type = COMEDI_SUBD_DIO;
	s->subdev_flags = SDF_READABLE | SDF_WRITABLE;
	s->n_chan = 8;
	s->maxdata = 1;
	s->range_table = &range_digital;
        s->insn_bits = apci_dio_insn_bits;
        s->insn_config = apci_dio_insn_config;

        /*--------------------------------  B  --------------------------------*/
	s = dev->subdevices + 1;
	
	s->type = COMEDI_SUBD_DIO;
	s->subdev_flags = SDF_READABLE | SDF_WRITABLE;
	s->n_chan = 8;
	s->maxdata = 1;
	s->range_table = &range_digital;
        s->insn_bits = apci_dio_insn_bits;
        s->insn_config = apci_dio_insn_config;
        
        /*--------------------------------  Chi  --------------------------------*/
	s = dev->subdevices + 2;

	s->type = COMEDI_SUBD_DIO;
	s->subdev_flags = SDF_READABLE | SDF_WRITABLE;
	s->n_chan = 4;
	s->maxdata = 1;
	s->range_table = &range_digital;
        s->insn_bits = apci_dio_insn_bits;
        s->insn_config = apci_dio_insn_config;	

        /*--------------------------------  Clo  --------------------------------*/
	s = dev->subdevices + 3;

	s->type = COMEDI_SUBD_DIO;
	s->subdev_flags = SDF_READABLE | SDF_WRITABLE;
	s->n_chan = 4;
	s->maxdata = 1;
	s->range_table = &range_digital;
        s->insn_bits = apci_dio_insn_bits;
        s->insn_config = apci_dio_insn_config;	

        /* Done */

	apci_debug("attached\n");

	return 0;
}


static int apci_detach(comedi_device * dev)
{
	apci_info("comedi%d: apci: remove\n", dev->minor);

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
		
		//outw(s->state,dev->iobase + SKEL_DIO);
	}

	
	//data[1]=inw(dev->iobase + SKEL_DIO);
	
	//data[1]=s->state;

	return 2;
}

static int apci_dio_insn_config(comedi_device * dev, comedi_subdevice * s,
	comedi_insn * insn, lsampl_t * data)
{
	int chan = CR_CHAN(insn->chanspec);
        apci_debug("Configuring...using channels: %d\n", chan );
	
	switch (data[0]) {
	case INSN_CONFIG_DIO_OUTPUT:
		s->io_bits |= 1 << chan;
		break;
	case INSN_CONFIG_DIO_INPUT:
		s->io_bits &= ~(1 << chan);
		break;
	case INSN_CONFIG_DIO_QUERY:
		data[1] =
			(s->
			io_bits & (1 << chan)) ? COMEDI_OUTPUT : COMEDI_INPUT;
		return insn->n;
		break;
	default:
		return -EINVAL;
		break;
	}
	//outw(s->io_bits,dev->iobase + SKEL_DIO_CONFIG);

	return insn->n;
}


COMEDI_INITCLEANUP(driver_skel);

// COMEDI_PCI_INITCLEANUP(driver_skel, apci_pci_table)
