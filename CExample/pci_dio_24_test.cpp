/**
 * Example of using commands - asynchronous input
 * Part of Comedilib
 * 
 * Example for testing the PC104idio
 *
 *
 */

#include <stdio.h>
#include <comedilib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "gtest/gtest.h"

extern comedi_t *device;

struct parsed_options
{
    char *filename;
    double value;
    int subdevice;
    int subdevice2;
    int channel;
    int aref;
    int range;
    int verbose;
    int n_chan;
    int n_scan;
    double freq;
    int dosomething;
};

/* #define BUFSZ 10000 */
/* char buf[BUFSZ]; */

#define N_CHANS 256
static unsigned int chanlist[N_CHANS];
static comedi_range * range_info[N_CHANS];
static lsampl_t maxdata[N_CHANS];


int prepare_cmd_lib(comedi_t *dev, int subdevice, int n_scan, int n_chan, unsigned period_nanosec, comedi_cmd *cmd);

void do_cmd(comedi_t *dev,comedi_cmd *cmd);

void print_datum(lsampl_t raw, int channel_index);

char *cmdtest_messages[]={
    (char *)"success",
    (char *)"invalid source",
    (char *)"source conflict",
    (char *)"invalid argument",
    (char *)"argument conflict",
    (char *)"invalid chanlist",
};
struct parsed_options options;
int subdev_flags;
comedi_t *dev;
comedi_cmd c,*cmd;
lsampl_t raw;
#define BUFSZ 1024
sampl_t buf[BUFSZ];

TEST(ComediBasic,AbleToDoSomething) {

}

#define COMEDI_DATA_READ(...) ret = comedi_data_read( __VA_ARGS__ );\
    ASSERT_TRUE(ret);
#define COMEDI_DIO_BITFIELD(...)  ret  = comedi_dio_bitfield( __VA_ARGS__);\
    ASSERT_TRUE( ret >= 0 );

/**
 * This test proves that we can read from the card.
 * Now, we want a way to verify that we can actually write to the card too !
 */
TEST(PCI_DIO_48,Test_read_auto_incrementing_digital ) {
}

#define COMEDI_DIO_BITFIELD2(...)   ret  = comedi_dio_bitfield2( __VA_ARGS__); \
    ASSERT_TRUE( ret >= 0 );


TEST(Pci_Dio_48,Test_writing_digital ) {
    // lsampl_t data;
    // int ret;
    // int subdev = 0;        
    // int chan = 0;          
    // int range = 0;         
    // int aref = AREF_GROUND;

    // dev = comedi_open(options.filename);
    // ASSERT_TRUE( dev );
    // subdev_flags = comedi_get_subdevice_flags(dev, options.subdevice);
    // ret = comedi_dio_config( dev, options.subdevice, options.channel, COMEDI_OUTPUT );
    // ASSERT_TRUE( ret >= 0 );
    // data = 0x0;
    // COMEDI_DIO_BITFIELD2(dev,options.subdevice, 0, &data , 0);
    // /* EXPECT_EQ( data, 0x00 ); */
    // data = 0x1;
    // COMEDI_DIO_BITFIELD2(dev,options.subdevice, 0, &data , 0);
    // /* EXPECT_EQ( data, 0x01 ); */
    // data = 0x23;
    // COMEDI_DIO_BITFIELD2(dev,options.subdevice, 0, &data , 0);
    // /* EXPECT_EQ( data, 0x02 ); */
    // comedi_close( dev );
}

TEST(Pci_Dio_48,TestDigital) {
}


TEST(Pci_Dio_48,TestDigital_again) {
}

                                          
int main(int argc, char *argv[])
{
	int ret;
	int total=0;
	int i;
        cmd =&c;
        /* init_parsed_options( &options ); */

	memset(&options, 0, sizeof(options));
	/* The following variables used in this demo
	 * can be modified by command line
	 * options.  When modifying this demo, you may want to
	 * change them here. */
	options.filename = (char *)"/dev/comedi0";
	options.subdevice = 0;
	options.subdevice2 = 1;
	options.channel = 1;
	options.range = 0;
	options.aref = AREF_GROUND;
	options.n_chan = 4;
	options.n_scan = 10000;
	options.freq = 5000.0;
        /* parse_options( &options, argc, argv ); */
	/* open the device */
	/* dev = comedi_open(options.filename); */
        ::testing::GTEST_FLAG(print_time) = false;

        testing::InitGoogleTest(&argc, argv);
        testing::TestEventListeners & listeners = testing::UnitTest::GetInstance()->listeners();
#ifdef GTEST_TAP_PRINT_TO_STDOUT
        delete listeners.Release(listeners.default_result_printer());
#endif
        
        listeners.Append( new tap::TapListener() );
        return RUN_ALL_TESTS();  



	return 0;
}

/*
 * This prepares a command in a pretty generic way.  We ask the
 * library to create a stock command that supports periodic
 * sampling of data, then modify the parts we want. */
int prepare_cmd_lib(comedi_t *dev, int subdevice, int n_scan, int n_chan, unsigned scan_period_nanosec, comedi_cmd *cmd)
{
	int ret;

	memset(cmd,0,sizeof(*cmd));

	/* This comedilib function will get us a generic timed
	 * command for a particular board.  If it returns -1,
	 * that's bad. */
	ret = comedi_get_cmd_generic_timed(dev, subdevice, cmd, n_chan, scan_period_nanosec);
	if (ret<0){
		printf("comedi_get_cmd_generic_timed failed\n");
		return ret;
	}

	/* Modify parts of the command */
	cmd->chanlist = chanlist;
	cmd->chanlist_len = 1;
	if (cmd->stop_src == TRIG_COUNT) cmd->stop_arg = n_scan;

	return 0;
}

void print_datum(lsampl_t raw, int channel_index) {
	double physical_value;
	physical_value = comedi_to_phys(raw, range_info[channel_index], maxdata[channel_index]);
	printf("%#8.6g ",physical_value);
}

// Print numbers for clipped inputs
/* comedi_set_global_oor_behavior(COMEDI_OOR_NUMBER);
/* Set up channel list */
/* for(i = 0; i < options.n_chan; i++){ */
/* 	chanlist[i] = CR_PACK(options.channel + i, options.range, options.aref); */
/* 	range_info[i] = comedi_get_range(dev, options.subdevice, options.channel, options.range); */
/* 	maxdata[i] = comedi_get_maxdata(dev, options.subdevice, options.channel); */
/* } */
/* prepare_cmd_lib() uses a Comedilib routine to find a
 * good command for the device.  prepare_cmd() explicitly
 * creates a command, which may not work for your device. */
/* prepare_cmd_lib(dev, options.subdevice, options.n_scan, options.n_chan, 1e9 / options.freq, cmd); */
/* comedi_command_test() tests a command to see if the
 * trigger sources and arguments are valid for the subdevice.
 * If a trigger source is invalid, it will be logically ANDed
 * with valid values (trigger sources are actually bitmasks),
 * which may or may not result in a valid trigger source.
 * If an argument is invalid, it will be adjusted to the
 * nearest valid value.  In this way, for many commands, you
 * can test it multiple times until it passes.  Typically,
 * if you can't get a valid command in two tests, the original
 * command wasn't specified very well. */
/* ret = comedi_command_test(dev, cmd); */
/* if(ret < 0){ */
/* 	comedi_perror("comedi_command_test"); */
/* 	if(errno == EIO){ */
/* 		fprintf(stderr,"Ummm... this subdevice doesn't support commands\n"); */
/* 	} */
/* 	exit(1); */
/* } */
/* ret = comedi_command_test(dev, cmd); */
/* if(ret < 0){ */
/* 	comedi_perror("comedi_command_test"); */
/* 	exit(1); */
/* } */
/* fprintf(stderr,"second test returned %d (%s)\n", ret, */
/* 		cmdtest_messages[ret]); */
/* if(ret!=0){ */
/* 	fprintf(stderr, "Error preparing command\n"); */
/* 	exit(1); */
/* } */
/* start the command */
/* ret = comedi_command(dev, cmd); */
/* if(ret < 0){ */
/* 	comedi_perror("comedi_command"); */
/* 	exit(1); */
/* } */


/* int ret; */
/* int total; */
/* int i; */
/* lsampl_t data; */
/* int subdev = 0;         */
/* int chan = 0;           */
/* int range = 0;          */
/* int aref = AREF_GROUND; */
/* dev = comedi_open(options.filename); */
/* subdev_flags = comedi_get_subdevice_flags(dev, options.subdevice); */
/* while(1){ */
/*     /\* ret = read(comedi_fileno(dev),buf,BUFSZ); *\/ */
/*     ASSERT_TRUE( dev ); */
/*     /\* if(!dev){ *\/ */
/*     /\*     comedi_perror(options.filename); *\/ */
/*     /\*     exit(1); *\/ */
/*     /\* } *\/ */
/*     /\* Must select this device first and then you can  */
/*        actually read from the correct digital device *\/ */
/*     ret = comedi_dio_config( dev, options.subdevice, options.channel, COMEDI_INPUT ); */

/*     unsigned bit = 0; */
/*     /\* rc = comedi_dio_config(device, subdevice, i, COMEDI_OUTPUT); *\/ */
/*     ret = comedi_dio_read(dev,0,2, &bit); */
/*     /\* ret = read( comedi_fileno(dev), buf, 2  ); *\/ */

/*     ret = comedi_data_read(dev,options.subdevice,0,range,aref, &data); */

/*     bit = 0xf1; */
/*     ret = comedi_dio_config( dev, options.subdevice, options.channel, COMEDI_OUTPUT ); */
/*     ret = comedi_dio_write( dev, 0, 3, bit ); */
    

/*     ret = comedi_data_write( dev, options.subdevice, 0, 0, 0, bit ); */
/*     bit = 0xf1; */

/*     /\* ret =  comedi_data_write(device, options.subdevice, channel,  range, , bit); *\/ */
   
/*     ret = write( comedi_fileno( dev ), buf , 2 ); */

/*     ret = write( comedi_fileno( dev ), buf , 1 ); */



/*     EXPECT_TRUE( ret >= 0 ); */
/*     if (ret < 0) { */
/*         /\* perror("read"); *\/ */
/*         break; */
/*     } else if (ret == 0){ */
/*         /\* reached stop condition *\/ */
/*         break; */
/*     } else { */
/*         static int col = 0; */
/*         int bytes_per_sample; */
/*         total += ret; */
/*         if (options.verbose)fprintf(stderr, "read %d %d\n", ret, total); */
/*         if (subdev_flags & SDF_LSAMPL) */
/*             bytes_per_sample = sizeof(lsampl_t); */
/*         else */
/*             bytes_per_sample = sizeof(sampl_t); */
/*         for(i = 0; i < ret / bytes_per_sample; i++){ */
/*             if (subdev_flags & SDF_LSAMPL) { */
/*                 raw = ((lsampl_t *)buf)[i]; */
/*             } else { */
/*                 raw = ((sampl_t *)buf)[i]; */
/*             } */
/*             /\* print_datum(raw, col); *\/ */
/*             print_datum(raw, col); */
/*             col++; */
/*             if ( col == options.n_chan ){ */
/*                 printf("\n"); */
/*                 col=0; */
/*             } */
/*         } */
/*     } */
/* } */
