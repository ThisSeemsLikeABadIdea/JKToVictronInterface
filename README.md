# JKToVictronInterface - Replaced 

I'm not sure what happened but this no longer works with Esspressif IDF, I have replaced this with a new version (to be uploaded shortly), im leaving it here for reference until i can replace it entirely

A very rough start on an JK BMS interface for Victron Energy Systems

This is not an arduino project, its an espressif IDF project. (IDF 4.4)

You will need to familizarize yourself with it in order to make this work. Its not particularly difficult if you use VS Code. Simply install VS Code and use the espressif idf installer to install the correct version. (it will default to the latest version) 


The intention is to have a standalone device that can communicate the JKBMS details to Victron, But when using multiple devices instead of having a master node as a single point of failure i would like all the devices *when running more than one to become the master and take over. We live off grid and cant afford for a single device to cause our power to go out. 

A fair bit of of this repo has been snatched from DIY BMS, ESPHOME and others. 

Can tranceiver is connected to 
 Can_TX_GPIO_NUM             21
 Can_RX_GPIO_NUM             22
 the model number is WCMCU-230  (https://www.ebay.com.au/itm/393502933357) these things are hit and miss, sometimes they work othertimes they are duds

 The serial to JK-BMS is connected to
  #define JK_TX_PIN 16
  #define JK_RX_PIN 17

The main.c file is the espressif IDF app and entry point. 
  the main.c creates a task to run an instance of JKBMS and that has its own loop to pull data from the JKBMS
  main.c create a task for the TWAI interface
