/* Copyright (C) 2004 Bart
 * Copyright (C) 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015 Curtis Gedak
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "Win_GParted.h"
#include "Dialog_Progress.h"
#include "DialogFeatures.h"
#include "Dialog_Disklabel.h"
#include "Dialog_Rescue_Data.h"
#include "Dialog_Partition_Resize_Move.h"
#include "Dialog_Partition_Copy.h"
#include "Dialog_Partition_New.h"
#include "Dialog_Partition_Info.h"
#include "Dialog_FileSystem_Label.h"
#include "Dialog_Partition_Name.h"
#include "DialogManageFlags.h"
#include "Mount_Info.h"
#include "OperationCopy.h"
#include "OperationCheck.h"
#include "OperationCreate.h"
#include "OperationDelete.h"
#include "OperationFormat.h"
#include "OperationResizeMove.h"
#include "OperationChangeUUID.h"
#include "OperationLabelFileSystem.h"
#include "OperationNamePartition.h"
#include "Partition.h"
#include "PartitionVector.h"
#include "LVM2_PV_Info.h"
#include "../config.h"

#include <gtkmm/aboutdialog.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/radiobuttongroup.h>
#include <gtkmm/main.h>
#include <gtkmm/separator.h>
#include <glibmm/ustring.h>

namespace GParted
{
	
Win_GParted::Win_GParted( const std::vector<Glib::ustring> & user_devices )
{
	copied_partition = NULL;
	selected_partition_ptr = NULL;
	new_count = 1;
	current_device = 0 ;
	OPERATIONSLIST_OPEN = true ;
	gparted_core .set_user_devices( user_devices ) ;
	
	MENU_NEW = TOOLBAR_NEW =
        MENU_DEL = TOOLBAR_DEL =
        MENU_RESIZE_MOVE = TOOLBAR_RESIZE_MOVE =
        MENU_COPY = TOOLBAR_COPY =
        MENU_PASTE = TOOLBAR_PASTE =
        MENU_FORMAT =
        MENU_TOGGLE_BUSY =
        MENU_MOUNT =
        MENU_NAME_PARTITION =
        MENU_FLAGS =
        MENU_INFO =
        MENU_LABEL_PARTITION =
        MENU_CHANGE_UUID =
        TOOLBAR_UNDO =
        TOOLBAR_APPLY = -1 ;

	//==== GUI =========================
	this ->set_title( _("GParted") );
	this ->set_default_size( 775, 500 );
	
	try
	{
#ifdef HAVE_SET_DEFAULT_ICON_NAME
		this ->set_default_icon_name( "gparted" ) ;
#else
		this ->set_icon_from_file( GNOME_ICONDIR "/gparted.png" ) ;
#endif
	}
	catch ( Glib::Exception & e )
	{
		std::cout << e .what() << std::endl ;
	}
	
	//Pack the main box
	this ->add( vbox_main ); 
	
	//menubar....
	init_menubar() ;
	vbox_main .pack_start( menubar_main, Gtk::PACK_SHRINK );
	
	//toolbar....
	init_toolbar() ;
	vbox_main.pack_start( hbox_toolbar, Gtk::PACK_SHRINK );
	
	//drawingarea_visualdisk...  ( contains the visual represenation of the disks )
	drawingarea_visualdisk .signal_partition_selected .connect( 
			sigc::mem_fun( this, &Win_GParted::on_partition_selected ) ) ;
	drawingarea_visualdisk .signal_partition_activated .connect( 
			sigc::mem_fun( this, &Win_GParted::on_partition_activated ) ) ;
	drawingarea_visualdisk .signal_popup_menu .connect( 
			sigc::mem_fun( this, &Win_GParted::on_partition_popup_menu ) );
	vbox_main .pack_start( drawingarea_visualdisk, Gtk::PACK_SHRINK ) ;
	
	//hpaned_main (NOTE: added to vpaned_main)
	init_hpaned_main() ;
	vpaned_main .pack1( hpaned_main, true, true ) ;
	
	//vpaned_main....
	vbox_main .pack_start( vpaned_main );
	
	//device info...
	init_device_info() ;
	
	//operationslist...
	hbox_operations .signal_undo .connect( sigc::mem_fun( this, &Win_GParted::activate_undo ) ) ;
	hbox_operations .signal_clear .connect( sigc::mem_fun( this, &Win_GParted::clear_operationslist ) ) ;
	hbox_operations .signal_apply .connect( sigc::mem_fun( this, &Win_GParted::activate_apply ) ) ;
	hbox_operations .signal_close .connect( sigc::mem_fun( this, &Win_GParted::close_operationslist ) ) ;
	vpaned_main .pack2( hbox_operations, true, true ) ;

	//statusbar... 
	pulsebar .set_pulse_step( 0.01 );
	statusbar .add( pulsebar );
	vbox_main .pack_start( statusbar, Gtk::PACK_SHRINK );
	
	this ->show_all_children();
	
	//make sure harddisk information is closed..
	hpaned_main .get_child1() ->hide() ;
}

Win_GParted::~Win_GParted()
{
	delete copied_partition;
	copied_partition = NULL;
}

void Win_GParted::init_menubar() 
{
	//fill menubar_main and connect callbacks 
	//gparted
	menu = manage( new Gtk::Menu() ) ;
	image = manage( new Gtk::Image( Gtk::Stock::REFRESH, Gtk::ICON_SIZE_MENU ) );
	menu ->items() .push_back( Gtk::Menu_Helpers::ImageMenuElem(
		_("_Refresh Devices"),
		Gtk::AccelKey("<control>r"),
		*image, 
		sigc::mem_fun(*this, &Win_GParted::menu_gparted_refresh_devices) ) );
	
	image = manage( new Gtk::Image( Gtk::Stock::HARDDISK, Gtk::ICON_SIZE_MENU ) );
	menu ->items() .push_back( Gtk::Menu_Helpers::ImageMenuElem( _("_Devices"), *image ) ) ; 
	
	menu ->items() .push_back( Gtk::Menu_Helpers::SeparatorElem( ) );
	menu ->items() .push_back( Gtk::Menu_Helpers::StockMenuElem( 
		Gtk::Stock::QUIT, sigc::mem_fun(*this, &Win_GParted::menu_gparted_quit) ) );
	menubar_main .items() .push_back( Gtk::Menu_Helpers::MenuElem( _("_GParted"), *menu ) );
	
	//edit
	menu = manage( new Gtk::Menu() ) ;
	menu ->items() .push_back( Gtk::Menu_Helpers::ImageMenuElem( 
		_("_Undo Last Operation"), 
		Gtk::AccelKey("<control>z"),
		* manage( new Gtk::Image( Gtk::Stock::UNDO, Gtk::ICON_SIZE_MENU ) ), 
		sigc::mem_fun(*this, &Win_GParted::activate_undo) ) );

	menu ->items() .push_back( Gtk::Menu_Helpers::ImageMenuElem( 
		_("_Clear All Operations"), 
		* manage( new Gtk::Image( Gtk::Stock::CLEAR, Gtk::ICON_SIZE_MENU ) ), 
		sigc::mem_fun(*this, &Win_GParted::clear_operationslist) ) );

	menu ->items() .push_back( Gtk::Menu_Helpers::ImageMenuElem( 
		_("_Apply All Operations"),
		Gtk::AccelKey(GDK_Return, Gdk::CONTROL_MASK),
		* manage( new Gtk::Image( Gtk::Stock::APPLY, Gtk::ICON_SIZE_MENU ) ), 
		sigc::mem_fun(*this, &Win_GParted::activate_apply) ) );
	menubar_main .items() .push_back( Gtk::Menu_Helpers::MenuElem( _("_Edit"), *menu ) );

	//view
	menu = manage( new Gtk::Menu() ) ;
	menu ->items() .push_back( Gtk::Menu_Helpers::CheckMenuElem(
		_("Device _Information"), sigc::mem_fun(*this, &Win_GParted::menu_view_harddisk_info) ) );
	menu ->items() .push_back( Gtk::Menu_Helpers::CheckMenuElem( 
		_("Pending _Operations"), sigc::mem_fun(*this, &Win_GParted::menu_view_operations) ) );
	menubar_main .items() .push_back( Gtk::Menu_Helpers::MenuElem( _("_View"), *menu ) );

	menu ->items() .push_back( Gtk::Menu_Helpers::SeparatorElem( ) );
	menu ->items() .push_back( Gtk::Menu_Helpers::MenuElem(
		_("_File System Support"), sigc::mem_fun( *this, &Win_GParted::menu_gparted_features ) ) );

	//device
	menu = manage( new Gtk::Menu() ) ;
	menu ->items() .push_back( Gtk::Menu_Helpers::MenuElem( Glib::ustring( _("_Create Partition Table") ) + "...",
								sigc::mem_fun(*this, &Win_GParted::activate_disklabel) ) );

	menu ->items() .push_back( Gtk::Menu_Helpers::MenuElem( Glib::ustring( _("_Attempt Data Rescue") ) + "...",
								sigc::mem_fun(*this, &Win_GParted::activate_attempt_rescue_data) ) );

	menubar_main .items() .push_back( Gtk::Menu_Helpers::MenuElem( _("_Device"), *menu ) );

	//partition
	init_partition_menu() ;
	menubar_main .items() .push_back( Gtk::Menu_Helpers::MenuElem( _("_Partition"), menu_partition ) );

	//help
	menu = manage( new Gtk::Menu() ) ;
	menu ->items() .push_back( Gtk::Menu_Helpers::ImageMenuElem( 
		_("_Contents"), 
		Gtk::AccelKey("F1"),
		* manage( new Gtk::Image( Gtk::Stock::HELP, Gtk::ICON_SIZE_MENU ) ), 
		sigc::mem_fun(*this, &Win_GParted::menu_help_contents) ) );
	menu ->items() .push_back( Gtk::Menu_Helpers::SeparatorElem( ) );
	menu ->items() .push_back( Gtk::Menu_Helpers::StockMenuElem(
		Gtk::Stock::ABOUT, sigc::mem_fun(*this, &Win_GParted::menu_help_about) ) );

	menubar_main.items() .push_back( Gtk::Menu_Helpers::MenuElem(_("_Help"), *menu ) );
}

void Win_GParted::init_toolbar() 
{
	int index = 0 ;
	//initialize and pack toolbar_main 
	hbox_toolbar.pack_start( toolbar_main );
	
	//NEW and DELETE
	image = manage( new Gtk::Image( Gtk::Stock::NEW, Gtk::ICON_SIZE_BUTTON ) );
	/*TO TRANSLATORS: "New" is a tool bar item for partition actions. */
	Glib::ustring str_temp = _("New") ;
	toolbutton = Gtk::manage(new Gtk::ToolButton( *image, str_temp ));
	toolbutton ->signal_clicked() .connect( sigc::mem_fun( *this, &Win_GParted::activate_new ) );
	toolbar_main .append( *toolbutton );
	TOOLBAR_NEW = index++ ;
	toolbutton ->set_tooltip(tooltips, _("Create a new partition in the selected unallocated space") );		
	toolbutton = Gtk::manage(new Gtk::ToolButton(Gtk::Stock::DELETE));
	toolbutton ->signal_clicked().connect( sigc::mem_fun(*this, &Win_GParted::activate_delete) );
	toolbar_main.append(*toolbutton);
	TOOLBAR_DEL = index++ ;
	toolbutton ->set_tooltip(tooltips, _("Delete the selected partition") );		
	toolbar_main.append( *(Gtk::manage(new Gtk::SeparatorToolItem)) );
	index++ ;
	
	//RESIZE/MOVE
	image = manage( new Gtk::Image( Gtk::Stock::GOTO_LAST, Gtk::ICON_SIZE_BUTTON ) );
	str_temp = _("Resize/Move") ;
	//Condition string split and Undo button.
	//  for longer translated string, split string in two and skip the Undo button to permit full toolbar to display
	//  FIXME:  Is there a better way to do this, perhaps without the conditional?  At the moment this seems to be the best compromise.
	bool display_undo = true ;
	if( str_temp .length() > 14 ) {
		size_t index = str_temp .find( "/" ) ;
		if ( index != Glib::ustring::npos ) {
			str_temp .replace( index, 1, "\n/" ) ;
			display_undo = false ;
		}
	}
	toolbutton = Gtk::manage(new Gtk::ToolButton( *image, str_temp ));
	toolbutton ->signal_clicked().connect( sigc::mem_fun(*this, &Win_GParted::activate_resize) );
	toolbar_main.append(*toolbutton);
	TOOLBAR_RESIZE_MOVE = index++ ;
	toolbutton ->set_tooltip(tooltips, _("Resize/Move the selected partition") );		
	toolbar_main.append( *(Gtk::manage(new Gtk::SeparatorToolItem)) );
	index++ ;

	//COPY and PASTE
	toolbutton = Gtk::manage(new Gtk::ToolButton(Gtk::Stock::COPY));
	toolbutton ->signal_clicked().connect( sigc::mem_fun(*this, &Win_GParted::activate_copy) );
	toolbar_main.append(*toolbutton);
	TOOLBAR_COPY = index++ ;
	toolbutton ->set_tooltip(tooltips, _("Copy the selected partition to the clipboard") );		
	toolbutton = Gtk::manage(new Gtk::ToolButton(Gtk::Stock::PASTE));
	toolbutton ->signal_clicked().connect( sigc::mem_fun(*this, &Win_GParted::activate_paste) );
	toolbar_main.append(*toolbutton);
	TOOLBAR_PASTE = index++ ;
	toolbutton ->set_tooltip(tooltips, _("Paste the partition from the clipboard") );		
	toolbar_main.append( *(Gtk::manage(new Gtk::SeparatorToolItem)) );
	index++ ;
	
	//UNDO and APPLY
	if ( display_undo ) {
		//Undo button is displayed only if translated language "Resize/Move" is not too long.  See above setting of this condition.
		toolbutton = Gtk::manage(new Gtk::ToolButton(Gtk::Stock::UNDO));
		toolbutton ->signal_clicked().connect( sigc::mem_fun(*this, &Win_GParted::activate_undo) );
		toolbar_main.append(*toolbutton);
		TOOLBAR_UNDO = index++ ;
		toolbutton ->set_sensitive( false );
		toolbutton ->set_tooltip(tooltips, _("Undo Last Operation") );
	}
	
	toolbutton = Gtk::manage(new Gtk::ToolButton(Gtk::Stock::APPLY));
	toolbutton ->signal_clicked().connect( sigc::mem_fun(*this, &Win_GParted::activate_apply) );
	toolbar_main.append(*toolbutton);
	TOOLBAR_APPLY = index++ ;
	toolbutton ->set_sensitive( false );
	toolbutton ->set_tooltip(tooltips, _("Apply All Operations") );		
	
	//initialize and pack combo_devices
	liststore_devices = Gtk::ListStore::create( treeview_devices_columns ) ;
	combo_devices .set_model( liststore_devices ) ;

	combo_devices .pack_start( treeview_devices_columns .icon, false ) ;
	combo_devices .pack_start( treeview_devices_columns .device ) ;
	combo_devices .pack_start( treeview_devices_columns .size, false ) ;
	
	combo_devices_changed_connection =
		combo_devices .signal_changed() .connect( sigc::mem_fun(*this, &Win_GParted::combo_devices_changed) );

	hbox_toolbar .pack_start( combo_devices, Gtk::PACK_SHRINK ) ;
}

void Win_GParted::init_partition_menu() 
{
	int index = 0 ;

	//fill menu_partition
	image = manage( new Gtk::Image( Gtk::Stock::NEW, Gtk::ICON_SIZE_MENU ) );
	menu_partition .items() .push_back( 
			/*TO TRANSLATORS: "_New" is a sub menu item for the partition menu. */
			Gtk::Menu_Helpers::ImageMenuElem( _("_New"),
							  Gtk::AccelKey( GDK_Insert, Gdk::BUTTON1_MASK),
							  *image,
							  sigc::mem_fun(*this, &Win_GParted::activate_new) ) );
	MENU_NEW = index++ ;
	
	menu_partition .items() .push_back( 
			Gtk::Menu_Helpers::StockMenuElem( Gtk::Stock::DELETE, 
							  Gtk::AccelKey( GDK_Delete, Gdk::BUTTON1_MASK ),
							  sigc::mem_fun(*this, &Win_GParted::activate_delete) ) );
	MENU_DEL = index++ ;

	menu_partition .items() .push_back( Gtk::Menu_Helpers::SeparatorElem() );
	index++ ;
	
	image = manage( new Gtk::Image( Gtk::Stock::GOTO_LAST, Gtk::ICON_SIZE_MENU ) );
	menu_partition .items() .push_back( 
			Gtk::Menu_Helpers::ImageMenuElem( _("_Resize/Move"), 
							  *image, 
							  sigc::mem_fun(*this, &Win_GParted::activate_resize) ) );
	MENU_RESIZE_MOVE = index++ ;
	
	menu_partition .items() .push_back( Gtk::Menu_Helpers::SeparatorElem() );
	index++ ;
	
	menu_partition .items() .push_back( 
			Gtk::Menu_Helpers::StockMenuElem( Gtk::Stock::COPY,
							  sigc::mem_fun(*this, &Win_GParted::activate_copy) ) );
	MENU_COPY = index++ ;
	
	menu_partition .items() .push_back( 
			Gtk::Menu_Helpers::StockMenuElem( Gtk::Stock::PASTE,
							  sigc::mem_fun(*this, &Win_GParted::activate_paste) ) );
	MENU_PASTE = index++ ;
	
	menu_partition .items() .push_back( Gtk::Menu_Helpers::SeparatorElem() );
	index++ ;
	
	image = manage( new Gtk::Image( Gtk::Stock::CONVERT, Gtk::ICON_SIZE_MENU ) );
	menu_partition .items() .push_back(
			/*TO TRANSLATORS: menuitem which holds a submenu with file systems.. */
			Gtk::Menu_Helpers::ImageMenuElem( _("_Format to"),
							  *image,
							  * create_format_menu() ) ) ;
	MENU_FORMAT = index++ ;
	
	menu_partition .items() .push_back( Gtk::Menu_Helpers::SeparatorElem() ) ;
	index++ ;
	
	menu_partition .items() .push_back(
			//This is a placeholder text. It will be replaced with some other text before it is used
			Gtk::Menu_Helpers::MenuElem( "--placeholder--",
						     sigc::mem_fun( *this, &Win_GParted::toggle_busy_state ) ) );
	MENU_TOGGLE_BUSY = index++ ;

	menu_partition .items() .push_back(
			/*TO TRANSLATORS: menuitem which holds a submenu with mount points.. */
			Gtk::Menu_Helpers::MenuElem( _("_Mount on"), * manage( new Gtk::Menu() ) ) ) ;
	MENU_MOUNT = index++ ;

	menu_partition .items() .push_back( Gtk::Menu_Helpers::SeparatorElem() ) ;
	index++ ;

	menu_partition.items().push_back(
			Gtk::Menu_Helpers::MenuElem( _("_Name Partition"),
			                             sigc::mem_fun( *this, &Win_GParted::activate_name_partition ) ) );
	MENU_NAME_PARTITION = index++;

	menu_partition .items() .push_back(
			Gtk::Menu_Helpers::MenuElem( _("M_anage Flags"),
						     sigc::mem_fun( *this, &Win_GParted::activate_manage_flags ) ) );
	MENU_FLAGS = index++ ;

	menu_partition .items() .push_back(
			Gtk::Menu_Helpers::MenuElem( _("C_heck"),
						     sigc::mem_fun( *this, &Win_GParted::activate_check ) ) );
	MENU_CHECK = index++ ;

	menu_partition .items() .push_back(
			Gtk::Menu_Helpers::MenuElem( _("_Label File System"),
			                             sigc::mem_fun( *this, &Win_GParted::activate_label_filesystem ) ) );
	MENU_LABEL_PARTITION = index++ ;

	menu_partition .items() .push_back(
			Gtk::Menu_Helpers::MenuElem( _("New UU_ID"),
						     sigc::mem_fun( *this, &Win_GParted::activate_change_uuid ) ) );
	MENU_CHANGE_UUID = index++ ;

	menu_partition .items() .push_back( Gtk::Menu_Helpers::SeparatorElem() ) ;
	index++ ;
	
	menu_partition .items() .push_back( 
			Gtk::Menu_Helpers::StockMenuElem( Gtk::Stock::DIALOG_INFO,
							  sigc::mem_fun(*this, &Win_GParted::activate_info) ) );
	MENU_INFO = index++ ;
	
	menu_partition .accelerate( *this ) ;  
}

//Create the Partition --> Format to --> (file system list) menu
Gtk::Menu * Win_GParted::create_format_menu()
{
	const std::vector<FS> & fss = gparted_core .get_filesystems() ;
	menu = manage( new Gtk::Menu() ) ;

	for ( unsigned int t = 0 ; t < fss .size() ; t++ )
	{
		if ( GParted_Core::supported_filesystem( fss[t].filesystem ) &&
		     fss[t].filesystem != FS_LUKS                               )
			create_format_menu_add_item( fss[t].filesystem, fss[t].create );
	}
	//Add cleared at the end of the list
	create_format_menu_add_item( FS_CLEARED, true ) ;

	return menu ;
}

//Add one entry to the Partition --> Format to --> (file system list) menu
void Win_GParted::create_format_menu_add_item( FILESYSTEM filesystem, bool activate )
{
	hbox = manage( new Gtk::HBox() ) ;
	//the colored square
	hbox ->pack_start( * manage( new Gtk::Image( Utils::get_color_as_pixbuf( filesystem, 16, 16 ) ) ),
	                   Gtk::PACK_SHRINK ) ;
	//the label...
	hbox ->pack_start( * Utils::mk_label( " " + Utils::get_filesystem_string( filesystem ) ),
	                   Gtk::PACK_SHRINK ) ;

	menu ->items() .push_back( * manage( new Gtk::MenuItem( *hbox ) ) ) ;
	if ( activate )
		menu ->items() .back() .signal_activate() .connect(
			sigc::bind<GParted::FILESYSTEM>( sigc::mem_fun( *this, &Win_GParted::activate_format ),
			                                 filesystem ) ) ;
	else
		menu ->items() .back() .set_sensitive( false ) ;
}

void Win_GParted::init_device_info()
{
	vbox_info.set_spacing( 5 );
	int top = 0, bottom = 1;
	
	//title
	vbox_info .pack_start( 
		* Utils::mk_label( " <b>" + static_cast<Glib::ustring>( _("Device Information") ) + "</b>" ),
		Gtk::PACK_SHRINK );
	
	//GENERAL DEVICE INFO
	table = manage( new Gtk::Table() ) ;
	table ->set_col_spacings( 10 ) ;
	
	//model
	table ->attach( * Utils::mk_label( " <b>" + static_cast<Glib::ustring>( _("Model:") ) + "</b>" ),
			0, 1,
			top, bottom,
			Gtk::FILL ) ;
	device_info .push_back( Utils::mk_label( "", true, false, true ) ) ;
	table ->attach( * device_info .back(), 1, 2, top++, bottom++, Gtk::FILL ) ;

	// Serial number
	table->attach( *Utils::mk_label( " <b>" + static_cast<Glib::ustring>( _("Serial:") ) + "</b>" ),
	               0, 1, top, bottom, Gtk::FILL );
	device_info.push_back( Utils::mk_label( "", true, false, true ) );
	table->attach( *device_info.back(), 1, 2, top++, bottom++, Gtk::FILL );

	//size
	table ->attach( * Utils::mk_label( " <b>" + static_cast<Glib::ustring>( _("Size:") ) + "</b>" ),
			0, 1,
			top, bottom,
			Gtk::FILL ) ;
	device_info .push_back( Utils::mk_label( "", true, false, true ) ) ;
	table ->attach( * device_info .back(), 1, 2, top++, bottom++, Gtk::FILL ) ;
	
	//path
	table ->attach( * Utils::mk_label( " <b>" + static_cast<Glib::ustring>( _("Path:") ) + "</b>" ),
			0, 1,
			top, bottom,
			Gtk::FILL ) ;
	device_info .push_back( Utils::mk_label( "", true, false, true ) ) ;
	table ->attach( * device_info .back(), 1, 2, top++, bottom++, Gtk::FILL ) ;
	
	vbox_info .pack_start( *table, Gtk::PACK_SHRINK );
	
	//DETAILED DEVICE INFO 
	top = 0 ; bottom = 1;
	table = manage( new Gtk::Table() ) ;
	table ->set_col_spacings( 10 ) ;
	
	//one blank line
	table ->attach( * Utils::mk_label( "" ), 1, 2, top++, bottom++, Gtk::FILL );
	
	//disktype
	table ->attach( * Utils::mk_label( " <b>" + static_cast<Glib::ustring>( _("Partition table:") ) + "</b>" ),
			0, 1,
			top, bottom,
			Gtk::FILL );
	device_info .push_back( Utils::mk_label( "", true, false, true ) ) ;
	table ->attach( * device_info .back(), 1, 2, top++, bottom++, Gtk::FILL ) ;
	
	//heads
	table ->attach( * Utils::mk_label( " <b>" + static_cast<Glib::ustring>( _("Heads:") ) + "</b>" ),
			0, 1,
			top, bottom,
			Gtk::FILL ) ;
	device_info .push_back( Utils::mk_label( "", true, false, true ) ) ;
	table ->attach( * device_info .back(), 1, 2, top++, bottom++, Gtk::FILL ) ;
	
	//sectors/track
	table ->attach( * Utils::mk_label( " <b>" + static_cast<Glib::ustring>( _("Sectors/track:") ) + "</b>" ),
			0, 1,
			top, bottom,
			Gtk::FILL ) ;
	device_info .push_back( Utils::mk_label( "", true, false, true ) ) ;
	table ->attach( * device_info .back(), 1, 2, top++, bottom++, Gtk::FILL );
	
	//cylinders
	table ->attach( * Utils::mk_label( " <b>" + static_cast<Glib::ustring>( _("Cylinders:") ) + "</b>" ),
			0, 1,
			top, bottom,
			Gtk::FILL ) ;
	device_info .push_back( Utils::mk_label( "", true, false, true ) ) ;
	table ->attach( * device_info .back(), 1, 2, top++, bottom++, Gtk::FILL ) ;
	
	//total sectors
	table ->attach( * Utils::mk_label( " <b>" + static_cast<Glib::ustring>( _("Total sectors:") ) + "</b>" ),
			0, 1,
			top, bottom,
			Gtk::FILL );
	device_info .push_back( Utils::mk_label( "", true, false, true ) ) ;
	table ->attach( * device_info .back(), 1, 2, top++, bottom++, Gtk::FILL ) ;

	//sector size
	table ->attach( * Utils::mk_label( " <b>" + static_cast<Glib::ustring>( _("Sector size:") ) + "</b>" ),
			0, 1,
			top, bottom,
			Gtk::FILL );
	device_info .push_back( Utils::mk_label( "", true, false, true ) ) ;
	table ->attach( * device_info .back(), 1, 2, top++, bottom++, Gtk::FILL ) ;

	vbox_info .pack_start( *table, Gtk::PACK_SHRINK );
}

void Win_GParted::init_hpaned_main() 
{
	//left scrollwindow (holds device info)
	scrollwindow = manage( new Gtk::ScrolledWindow() ) ;
	scrollwindow ->set_shadow_type( Gtk::SHADOW_ETCHED_IN );
	scrollwindow ->set_policy( Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC );

	hpaned_main .pack1( *scrollwindow, true, true );
	scrollwindow ->add( vbox_info );

	//right scrollwindow (holds treeview with partitions)
	scrollwindow = manage( new Gtk::ScrolledWindow() ) ;
	scrollwindow ->set_shadow_type( Gtk::SHADOW_ETCHED_IN );
	scrollwindow ->set_policy( Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC );
	
	//connect signals and add treeview_detail
	treeview_detail .signal_partition_selected .connect( sigc::mem_fun( this, &Win_GParted::on_partition_selected ) );
	treeview_detail .signal_partition_activated .connect( sigc::mem_fun( this, &Win_GParted::on_partition_activated ) );
	treeview_detail .signal_popup_menu .connect( sigc::mem_fun( this, &Win_GParted::on_partition_popup_menu ) );
	scrollwindow ->add( treeview_detail );
	hpaned_main .pack2( *scrollwindow, true, true );
}

void Win_GParted::refresh_combo_devices()
{
	// Temporarily block the on change callback while re-creating the device list
	// behind the combobox to prevent flashing redraw by being redrawn with an empty
	// device list.
	combo_devices_changed_connection .block();
	liststore_devices ->clear() ;
	
	menu = manage( new Gtk::Menu() ) ;
	Gtk::RadioButtonGroup radio_group ;
	
	for ( unsigned int i = 0 ; i < devices .size( ) ; i++ )
	{
		//combo...
		treerow = *( liststore_devices ->append() ) ;
		treerow[ treeview_devices_columns .icon ] =
			render_icon( Gtk::Stock::HARDDISK, Gtk::ICON_SIZE_LARGE_TOOLBAR ) ;
		treerow[ treeview_devices_columns .device ] = devices[ i ] .get_path() ;
		treerow[ treeview_devices_columns .size ] = "(" + Utils::format_size( devices[ i ] .length, devices[ i ] .sector_size ) + ")" ; 
	
		//devices submenu....
		hbox = manage( new Gtk::HBox() ) ;
		hbox ->pack_start( * Utils::mk_label( devices[ i ] .get_path() ), Gtk::PACK_EXPAND_WIDGET ) ;
		hbox ->pack_start( * Utils::mk_label( "   (" + Utils::format_size( devices[ i ] .length, devices[ i ] .sector_size ) + ")" ),
		                   Gtk::PACK_SHRINK ) ;

		menu ->items() .push_back( * manage( new Gtk::RadioMenuItem( radio_group ) ) ) ;
		menu ->items() .back() .add( *hbox ) ;
		menu ->items() .back() .signal_activate() .connect( 
			sigc::bind<unsigned int>( sigc::mem_fun(*this, &Win_GParted::radio_devices_changed), i ) ) ;
	}
				
	menubar_main .items()[ 0 ] .get_submenu() ->items()[ 1 ] .remove_submenu() ;

	if ( menu ->items() .size() )
	{
		menu ->show_all() ;
		menubar_main .items()[ 0 ] .get_submenu() ->items()[ 1 ] .set_submenu( *menu ) ;
	}

	combo_devices_changed_connection .unblock();
	combo_devices .set_active( current_device ) ;
}

bool Win_GParted::pulsebar_pulse()
{
	pulsebar.pulse();
	Glib::ustring tmp_msg = gparted_core .get_thread_status_message() ;
	if ( tmp_msg != "" ) {
		statusbar.pop();
		statusbar.push( tmp_msg );
	}

	return true;
}

void Win_GParted::show_pulsebar( const Glib::ustring & status_message ) 
{
	pulsebar .show();
	statusbar .push( status_message) ;
	
	//disable all input stuff
	toolbar_main .set_sensitive( false ) ;
	menubar_main .set_sensitive( false ) ;
	combo_devices .set_sensitive( false ) ;
	menu_partition .set_sensitive( false ) ;
	treeview_detail .set_sensitive( false ) ;
	drawingarea_visualdisk .set_sensitive( false ) ;
		
	// connect pulse update timer
	pulsetimer = Glib::signal_timeout().connect( sigc::mem_fun(*this, &Win_GParted::pulsebar_pulse), 100 );
}

void Win_GParted::hide_pulsebar()
{
	pulsetimer.disconnect();
	pulsebar .hide();
	statusbar .pop() ;
		
	//enable all disabled stuff
	toolbar_main .set_sensitive( true ) ;
	menubar_main .set_sensitive( true ) ;
	combo_devices .set_sensitive( true ) ;
	menu_partition .set_sensitive( true ) ;
	treeview_detail .set_sensitive( true ) ;
	drawingarea_visualdisk .set_sensitive( true ) ;
}

void Win_GParted::Fill_Label_Device_Info( bool clear ) 
{
	if ( clear )
		for ( unsigned int t = 0 ; t < device_info .size( ) ; t++ )
			device_info[ t ] ->set_text( "" ) ;
		
	else
	{		
		short t = 0;
		
		//global info...
		device_info[ t++ ] ->set_text( devices[ current_device ] .model ) ;
		device_info[ t++ ] ->set_text( devices[current_device].serial_number );
		device_info[ t++ ] ->set_text( Utils::format_size( devices[ current_device ] .length, devices[ current_device ] .sector_size ) ) ;
		device_info[ t++ ] ->set_text( devices[current_device].get_path() );

		//detailed info
		device_info[ t++ ] ->set_text( devices[ current_device ] .disktype ) ;
		device_info[ t++ ] ->set_text( Utils::num_to_str( devices[ current_device ] .heads ) );
		device_info[ t++ ] ->set_text( Utils::num_to_str( devices[ current_device ] .sectors ) );
		device_info[ t++ ] ->set_text( Utils::num_to_str( devices[ current_device ] .cylinders ) );
		device_info[ t++ ] ->set_text( Utils::num_to_str( devices[ current_device ] .length ) );
		device_info[ t++ ] ->set_text( Utils::num_to_str( devices[ current_device ] .sector_size ) );
	}
}

bool Win_GParted::on_delete_event( GdkEventAny *event )
{
	return ! Quit_Check_Operations();
}	

void Win_GParted::Add_Operation( Operation * operation )
{
	if ( operation )
	{ 
		Glib::ustring error ;
		//Add any of the listed operations without further checking, but
		//  for the other operations (_CREATE, _RESIZE_MOVE and _COPY)
		//  ensure the partition is correctly aligned.
		//FIXME: this is becoming a mess.. maybe it's better to check if partition_new > 0
		if ( operation ->type == OPERATION_DELETE ||
		     operation ->type == OPERATION_FORMAT ||
		     operation ->type == OPERATION_CHECK ||
		     operation ->type == OPERATION_CHANGE_UUID ||
		     operation ->type == OPERATION_LABEL_FILESYSTEM ||
		     operation ->type == OPERATION_NAME_PARTITION ||
		     gparted_core.snap_to_alignment( operation->device, operation->get_partition_new(), error )
		   )
		{
			operation ->create_description() ;
			operations.push_back( operation );
		}
		else
		{
			Gtk::MessageDialog dialog( *this,
				   _("Could not add this operation to the list"),
				   false,
				   Gtk::MESSAGE_ERROR,
				   Gtk::BUTTONS_OK,
				   true );
			dialog .set_secondary_text( error ) ;

			dialog .run() ;
		}
	}
}

// Try to merge the second operation into the first in the operations[] vector.
bool Win_GParted::merge_two_operations( unsigned int first, unsigned int second )
{
	unsigned int num_ops = operations.size();
	if ( first >= num_ops-1 )
		return false;
	if ( first >= second || second >= num_ops )
		return false;

	if ( operations[first]->merge_operations( *operations[second] ) )
	{
		remove_operation( second );
		return true;
	}

	return false;
}

// Try to merge pending operations in the operations[] vector using the specified merge
// type.
//
// Summary of all the operation merging rules for each operation type coded into the
// ::activate_*() methods:
//
// Operation type      Partition status    Merge type             Method
// -----------------   ----------------    --------------------   -----------------
// resize/move         Real                MERGE_LAST_WITH_PREV   activate_resize()
// resize/move         New                 MERGE_LAST_WITH_ANY    activate_resize()
// paste               *                   none                   activate_paste()
// new                 *                   none                   activate_new()
// delete              Real                none                   activate_delete()
// delete              New                 MERGE_ALL_ADJACENT     activate_delete()
// format              Real                MERGE_LAST_WITH_PREV   activate_format()
// format              New                 MERGE_LAST_WITH_ANY    activate_format()
// check               Real [1]            MERGE_LAST_WITH_ANY    activate_check()
// label file system   Real [1]            MERGE_LAST_WITH_ANY    activate_label_filesystem()
// name partition      Real [1]            MERGE_LAST_WITH_ANY    activate_name_partition()
// new UUID            Real [1]            MERGE_LAST_WITH_ANY    activate_change_uuid()
//
// [1] The UI only allows these operations to be applied to real partitions; where as the
//     other mergeable operations can be applied to both real partitions and new, pending
//     create partitions.
void Win_GParted::merge_operations( MergeType mergetype )
{
	unsigned int num_ops = operations.size();
	if ( num_ops <= 1 )
		return;  // Nothing to merge.  One or fewer operations.

	switch ( mergetype )
	{
		case MERGE_LAST_WITH_PREV:
			merge_two_operations( num_ops-2, num_ops-1 );
			break;

		case MERGE_LAST_WITH_ANY:
			for ( unsigned int i = 0 ; i < num_ops-1 ; i ++ )
			{
				if ( merge_two_operations( i, num_ops-1 ) )
					break;
			}
			break;

		case MERGE_ALL_ADJACENT:
			// Must check against operations.size() as looping continues after
			// merging which might have reduced the number of items in the
			// vector.
			for ( unsigned int i = 0 ; i < operations.size()-1 ; i ++ )
			{
				merge_two_operations( i, i+1 );
			}
			break;
	}
}

void Win_GParted::Refresh_Visual()
{
	// How GParted displays partitions in the GUI and manages the lifetime and
	// ownership of that data:
	//
	// (1) Queries the devices and partitions on disk and refreshes the model.
	//
	//     Data owner: std::vector<Devices> Win_GParted::devices
	//     Lifetime:   Valid until the next call to Refresh_Visual()
	//     Call chain:
	//
	//         Win_GParted::menu_gparted_refresh_devices()
	//             gparted_core.set_devices( devices )
	//                 GParted_Core::set_devices_thread( devices )
	//                     devices.clear()
	//                     etc.
	//
	// (2) Takes a copy of the partitions for the device currently being shown in the
	//     GUI and visually applies pending operations.
	//
	//     Data owner: PartitionVector Win_GParted::display_partitions
	//     Lifetime:   Valid until the next call to Refresh_Visual().
	//     Function:   Refresh_Visual()
	//
	// (3) Loads the disk graphic and partition list with partitions to be shown in
	//     the GUI.  Both classes store pointers pointing back to each partition
	//     object in the vector of display partitions.
	//
	//     Aliases:   Win_GParted::display_partitions[]
	//     Call chain:
	//
	//         Win_GParted::Refresh_Visual()
	//             drawingarea_visualdisk.load_partitions( display_partitions, device_sectors )
	//                 DrawingAreaVisualDisk::set_static_data( ... )
	//             treeview_detail.load_partitions( display_partitions )
	//                 TreeView_Detail::create_row()
	//                 TreeView_Detail::load_partitions()
	//                     TreeView_Detail::create_row()
	//
	// (4) Selecting a partition in the disk graphic or in the partition list fires
	//     the callback which passes a pointer to the selected partition stored in
	//     step (3).  The callback saves the selected partition and calls the opposite
	//     class to update it's selection.
	//
	//     Data owner: const Partition * Win_GParted::selected_partition_ptr
	//     Aliases:    Win_GParted::display_partitions[]
	//     Lifetime:   Valid until the next call to Refresh_Visual().
	//     Call chain: (example clicking on a partition in the disk graphic)
	//
	//         DrawingAreaVisualDisk::on_button_press_event()
	//             DawingAreaVisualDisk::set_selected( visual_partitions, x, y )
	//                 signal_partition_selected.emit( ..., false )
	//                     Win_GParted::on_partition_selected( partition_ptr, src_is_treeview )
	//                         treeview_detail.set_selected( treestore_detail->children(), partition_ptr )
	//                             TreeView::set_selected( rows, partition_ptr, inside_extended )
	//                                 set_cursor()
	//                                 TreeView::set_selected( rows, partition_ptr, true )
	//                                     set_cursor()
	//
	// (5) Each new operation is added to the vector of pending operations.
	//     Eventually Refresh_Visual() is call to update the GUI.  This goes to step
	//     (2) which visually reapplies all pending operations again, including the
	//     newly added operation.
	//
	//     Data owner: std::vector<Operation *> Win_GParted::operations
	//     Lifetime:   Valid until operations have been applied by
	//                 GParted_Core::apply_operation_to_disk().  Specifically longer
	//                 than the next call call to Refresh_Visual().
	//     Call chain: (example setting a file system label)
	//
	//         Win_GParted::activate_label_filesystem()
	//             Win_GParted::Add_Operation( operation )
	//             Win_GParted::merge_operations( ... )
	//             Win_GParted::show_operationslist()
	//                 Win_GParted::Refresh_Visual()
	//
	// (6) Selecting a partition as a copy source makes a copy of that partition
	//     object.
	//
	//     Data owner: Partition Win_GParted::copied_partition
	//     Lifetime:   Valid until GParted closed or the device is removed.
	//                 Specifically longer than the next call to Refresh_Visual().
	//     Function:   Win_GParted::activate_copy()

	display_partitions = devices[current_device].partitions;

	//make all operations visible
	for ( unsigned int t = 0 ; t < operations .size(); t++ )
		if ( operations[ t ] ->device == devices[ current_device ] )
			operations[t]->apply_to_visual( display_partitions );
			
	hbox_operations .load_operations( operations ) ;

	//set new statusbartext
	statusbar .pop() ;
	statusbar .push( String::ucompose( ngettext( "%1 operation pending"
	                                           , "%1 operations pending"
	                                           , operations .size()
	                                           )
	                                 , operations .size()
	                                 )
	               );
		
	if ( ! operations .size() ) 
		allow_undo_clear_apply( false ) ;

	// Refresh copy partition source as necessary and select the largest unallocated
	// partition if there is one.
	selected_partition_ptr = NULL;
	Sector largest_unalloc_size = -1 ;
	Sector current_size ;

	for ( unsigned int t = 0 ; t < display_partitions.size() ; t++ )
	{
		if ( copied_partition != NULL && display_partitions[t].get_path() == copied_partition->get_path() )
		{
			delete copied_partition;
			copied_partition = display_partitions[t].clone();
		}

		switch ( display_partitions[t].type )
		{
			case TYPE_EXTENDED:
				for ( unsigned int u = 0 ; u < display_partitions[t].logicals.size() ; u ++ )
				{
					if ( copied_partition != NULL &&
					     display_partitions[t].logicals[u].get_path() == copied_partition->get_path() )
					{
						delete copied_partition;
						copied_partition = display_partitions[t].logicals[u].clone();
					}

					switch ( display_partitions[t].logicals[u].type )
					{
						case TYPE_UNALLOCATED:
							current_size = display_partitions[t].logicals[u].get_sector_length();
							if ( current_size > largest_unalloc_size )
							{
								largest_unalloc_size = current_size ;
								selected_partition_ptr = & display_partitions[t].logicals[u];
							}
							break;

						default:
							break;
					}
				}
				break;

			case TYPE_UNALLOCATED:
				current_size = display_partitions[t].get_sector_length();
				if ( current_size > largest_unalloc_size )
				{
					largest_unalloc_size = current_size ;
					selected_partition_ptr = & display_partitions[t];
				}
				break;

			default				:
				break;
		}
	}

	// frame visualdisk
	drawingarea_visualdisk.load_partitions( display_partitions, devices[current_device].length );

	// treeview details
	treeview_detail.load_partitions( display_partitions );

	set_valid_operations() ;

	// Process Gtk events to redraw visuals with reloaded partition details
	while ( Gtk::Main::events_pending() )
		Gtk::Main::iteration();

	if ( largest_unalloc_size >= 0 )
	{
		// Flashing redraw work around.  Inform visuals of selection of the
		// largest unallocated partition after drawing those visuals above.
		drawingarea_visualdisk.set_selected( selected_partition_ptr );
		treeview_detail.set_selected( selected_partition_ptr );

		// Process Gtk events to draw selection
		while ( Gtk::Main::events_pending() )
			Gtk::Main::iteration();
	}
}

// Confirms that the pointer points to one of the partition objects in the vector of
// displayed partitions, Win_GParted::display_partitions[].
// Usage: g_assert( valid_display_partition_ptr( my_partition_ptr ) );
bool Win_GParted::valid_display_partition_ptr( const Partition * partition_ptr )
{
	for ( unsigned int i = 0 ; i < display_partitions.size() ; i++ )
	{
		if ( & display_partitions[i] == partition_ptr )
			return true;
		else if ( display_partitions[i].type == TYPE_EXTENDED )
		{
			for ( unsigned int j = 0 ; j < display_partitions[i].logicals.size() ; j++ )
			{
				if ( & display_partitions[i].logicals[j] == partition_ptr )
					return true;
			}
		}
	}
	return false;
}

bool Win_GParted::Quit_Check_Operations()
{
	if ( operations .size() )
	{
		Gtk::MessageDialog dialog( *this,
					   _("Quit GParted?"),
					   false,
					   Gtk::MESSAGE_QUESTION,
					   Gtk::BUTTONS_NONE,
					   true );

		dialog .set_secondary_text( String::ucompose( ngettext( "%1 operation is currently pending."
		                                                      , "%1 operations are currently pending."
		                                                      , operations .size()
		                                                      )
		                                            , operations .size()
		                                            )
		                          ) ;
	
		dialog .add_button( Gtk::Stock::QUIT, Gtk::RESPONSE_CLOSE );
		dialog .add_button( Gtk::Stock::CANCEL,Gtk::RESPONSE_CANCEL );
		
		if ( dialog .run() == Gtk::RESPONSE_CANCEL )
			return false;//don't close GParted
	}

	return true; //close GParted
}

void Win_GParted::set_valid_operations()
{
	allow_new( false ); allow_delete( false ); allow_resize( false ); allow_copy( false );
	allow_paste( false ); allow_format( false ); allow_toggle_busy_state( false ) ;
	allow_name_partition( false ); allow_manage_flags( false ); allow_check( false );
	allow_label_filesystem( false ); allow_change_uuid( false ); allow_info( false );

	dynamic_cast<Gtk::Label*>( menu_partition .items()[ MENU_TOGGLE_BUSY ] .get_child() )
		->set_label( FileSystem::get_generic_text ( CTEXT_DEACTIVATE_FILESYSTEM ) ) ;

	menu_partition .items()[ MENU_TOGGLE_BUSY ] .show() ;
	menu_partition .items()[ MENU_MOUNT ] .hide() ;	

	// No partition selected ...
	if ( ! selected_partition_ptr )
		return ;
	g_assert( valid_display_partition_ptr( selected_partition_ptr ) );  // Bug: Not pointing at a valid display partition object

	// Reference to the Partition object directly containing the file system.
	const Partition & selected_filesystem = selected_partition_ptr->get_filesystem_partition();

	// Get file system and LUKS encryption capabilities
	const FS & fs_cap = gparted_core.get_fs( selected_filesystem.filesystem );
	const FS & enc_cap = gparted_core.get_fs( FS_LUKS );

	//if there's something, there's some info ;)
	allow_info( true ) ;

	// Set an appropriate name for the activate/deactivate menu item.
	const FileSystem * filesystem_object = gparted_core.get_filesystem_object( selected_filesystem.filesystem );
	if ( filesystem_object )
		dynamic_cast<Gtk::Label*>( menu_partition .items()[ MENU_TOGGLE_BUSY ] .get_child() )
			->set_label( filesystem_object->get_custom_text(   selected_filesystem.busy
			                                                 ? CTEXT_DEACTIVATE_FILESYSTEM
			                                                 : CTEXT_ACTIVATE_FILESYSTEM ) );
	else
		dynamic_cast<Gtk::Label*>( menu_partition .items()[ MENU_TOGGLE_BUSY ] .get_child() )
			->set_label( FileSystem::get_generic_text (  selected_filesystem.busy
			                                           ? CTEXT_DEACTIVATE_FILESYSTEM
			                                           : CTEXT_ACTIVATE_FILESYSTEM )
			                                          ) ;

	// Only permit file system mount/unmount and swapon/swapoff when available
	if (    selected_partition_ptr->status == STAT_REAL
	     && selected_partition_ptr->type   != TYPE_EXTENDED
	     && selected_filesystem.filesystem != FS_LVM2_PV
	     && selected_filesystem.filesystem != FS_LINUX_SWRAID
	     && selected_filesystem.filesystem != FS_LUKS
	     && (    selected_filesystem.busy
	          || selected_filesystem.get_mountpoints().size() /* Have mount point(s) */
	          || selected_filesystem.filesystem == FS_LINUX_SWAP
	        )
	   )
		allow_toggle_busy_state( true ) ;

	// Only permit LVM VG activate/deactivate if the PV is busy or a member of a VG.
	// For now specifically allow activation of an exported VG, which LVM will fail
	// with "Volume group "VGNAME" is exported", otherwise user won't know why the
	// inactive PV can't be activated.
	if (    selected_partition_ptr->status == STAT_REAL
	     && selected_filesystem.filesystem == FS_LVM2_PV      // Active VGNAME from mount point
	     && ( selected_filesystem.busy || selected_filesystem.get_mountpoints().size() > 0 )
	   )
		allow_toggle_busy_state( true ) ;

	// Allow naming on devices that support it
	if ( selected_partition_ptr->type   != TYPE_UNALLOCATED   &&
	     selected_partition_ptr->status == STAT_REAL          &&
	     devices[current_device].partition_naming_supported()    )
		allow_name_partition( true );

	// Manage flags
	if ( selected_partition_ptr->type   != TYPE_UNALLOCATED &&
	     selected_partition_ptr->status == STAT_REAL        &&
	     ! selected_partition_ptr->whole_device                )
		allow_manage_flags( true );

#ifdef ENABLE_ONLINE_RESIZE
	// Online resizing always required the ability to update the partition table ...
	if ( ! devices[current_device].readonly &&
	     selected_filesystem.busy              )
	{
		// Can the plain file system be online resized?
		if ( selected_partition_ptr->filesystem != FS_LUKS  &&
		     ( fs_cap.online_grow || fs_cap.online_shrink )    )
			allow_resize( true );
		// Is resizing an open LUKS mapping and the online file system within
		// supported?
		if ( selected_partition_ptr->filesystem == FS_LUKS            &&
		     selected_partition_ptr->busy                             &&
		     ( ( enc_cap.online_grow && fs_cap.online_grow )     ||
		       ( enc_cap.online_shrink && fs_cap.online_shrink )    )    )
			allow_resize( true );
	}
#endif

	// Only unmount/swapoff/VG deactivate or online actions allowed if busy
	if ( selected_filesystem.busy )
		return ;

	// UNALLOCATED
	if ( selected_partition_ptr->type == TYPE_UNALLOCATED )
	{
		allow_new( true );
		
		// Find out if there is a partition to be copied and if it fits inside this unallocated space
		// FIXME:
		// Temporarily disable copying of encrypted content into new partitions
		// which can't yet be encrypted, until full LUKS read-write support is
		// implemented.
		if ( copied_partition             != NULL    &&
		     ! devices[current_device].readonly      &&
		     copied_partition->filesystem != FS_LUKS    )
		{
			const Partition & copied_filesystem_ptn = copied_partition->get_filesystem_partition();
			Byte_Value required_size ;
			if ( copied_filesystem_ptn.filesystem == FS_XFS )
				required_size = copied_filesystem_ptn.estimated_min_size() *
				                copied_filesystem_ptn.sector_size;
			else
				required_size = copied_filesystem_ptn.get_byte_length();

			//Determine if space is needed for the Master Boot Record or
			//  the Extended Boot Record.  Generally an an additional track or MEBIBYTE
			//  is required so for our purposes reserve a MEBIBYTE in front of the partition.
			//  NOTE:  This logic also contained in Dialog_Base_Partition::MB_Needed_for_Boot_Record
			if (   (   selected_partition_ptr->inside_extended
			        && selected_partition_ptr->type == TYPE_UNALLOCATED
			       )
			    || ( selected_partition_ptr->type == TYPE_LOGICAL )
			                                     /* Beginning of disk device */
			    || ( selected_partition_ptr->sector_start <= (MEBIBYTE / selected_partition_ptr->sector_size) )
			   )
				required_size += MEBIBYTE;

			//Determine if space is needed for the Extended Boot Record for a logical partition
			//  after this partition.  Generally an an additional track or MEBIBYTE
			//  is required so for our purposes reserve a MEBIBYTE in front of the partition.
			if (   (   (   selected_partition_ptr->inside_extended
			            && selected_partition_ptr->type == TYPE_UNALLOCATED
			           )
			        || ( selected_partition_ptr->type == TYPE_LOGICAL )
			       )
			    && ( selected_partition_ptr->sector_end
			         < ( devices[ current_device ] .length
			             - ( 2 * MEBIBYTE / devices[ current_device ] .sector_size )
			           )
			       )
			   )
				required_size += MEBIBYTE;

			//Determine if space is needed for the backup partition on a GPT partition table
			if (   ( devices[ current_device ] .disktype == "gpt" )
			    && ( ( devices[current_device].length - selected_partition_ptr->sector_end )
			         < ( MEBIBYTE / devices[ current_device ] .sector_size )
			       )
			   )
				required_size += MEBIBYTE ;

			if ( required_size <= selected_partition_ptr->get_byte_length() )
				allow_paste( true ) ;
		}
		
		return ;
	}
	
	// EXTENDED
	if ( selected_partition_ptr->type == TYPE_EXTENDED )
	{
		// Deletion is only allowed when there are no logical partitions inside.
		if ( selected_partition_ptr->logicals.size()      == 1                &&
		     selected_partition_ptr->logicals.back().type == TYPE_UNALLOCATED    )
			allow_delete( true ) ;
		
		if ( ! devices[ current_device ] .readonly )
			allow_resize( true ) ; 

		return ;
	}	
	
	// PRIMARY and LOGICAL
	if (  selected_partition_ptr->type == TYPE_PRIMARY || selected_partition_ptr->type == TYPE_LOGICAL )
	{
		allow_format( true ) ;

		// only allow deletion of partitions within a partition table
		// Also exclude open LUKS mappings until open/close is supported
		if ( ! selected_partition_ptr->whole_device & ! selected_partition_ptr->busy )
			allow_delete( true );

		// Resizing/moving always requires the ability to update the partition
		// table ...
		if ( ! devices[current_device].readonly )
		{
			// Can the plain file system be resized or moved?
			if ( selected_partition_ptr->filesystem != FS_LUKS   &&
			     ( fs_cap.grow || fs_cap.shrink || fs_cap.move )    )
				allow_resize( true );
			// Is growing or moving this closed LUKS mapping permitted?
			if ( selected_partition_ptr->filesystem == FS_LUKS &&
			     ! selected_partition_ptr->busy                &&
			     ( enc_cap.grow || enc_cap.move )                 )
				allow_resize( true );
			// Is resizing an open LUKS mapping and the file system within
			// supported?
			if ( selected_partition_ptr->filesystem == FS_LUKS     &&
			     selected_partition_ptr->busy                      &&
	                     ( ( enc_cap.online_grow && fs_cap.grow )     ||
			       ( enc_cap.online_shrink && fs_cap.shrink )    )    )
				allow_resize( true );
		}

		// Only allow copying of real partitions, excluding closed encryption
		// (which are only copied while open).
		if ( selected_partition_ptr->status == STAT_REAL &&
		     selected_filesystem.filesystem != FS_LUKS   &&
		     fs_cap.copy                                    )
			allow_copy( true ) ;
		
		//only allow labelling of real partitions that support labelling
		if ( selected_partition_ptr->status == STAT_REAL && fs_cap.write_label )
			allow_label_filesystem( true );

		//only allow changing UUID of real partitions that support it
		if ( selected_partition_ptr->status == STAT_REAL && fs_cap.write_uuid )
			allow_change_uuid( true ) ;

		// Generate Mount on submenu, except for LVM2 PVs borrowing mount point to
		// display the VGNAME and read-only supported LUKS.
		if ( selected_filesystem.filesystem != FS_LVM2_PV &&
		     selected_filesystem.filesystem != FS_LUKS    &&
		     selected_filesystem.get_mountpoints().size()    )
		{
			menu = menu_partition .items()[ MENU_MOUNT ] .get_submenu() ;
			menu ->items() .clear() ;
			std::vector<Glib::ustring> temp_mountpoints = selected_filesystem.get_mountpoints();
			for ( unsigned int t = 0 ; t < temp_mountpoints.size() ; t++ )
			{
				menu ->items() .push_back( 
					Gtk::Menu_Helpers::MenuElem( 
						temp_mountpoints[t],
						sigc::bind<unsigned int>( sigc::mem_fun(*this, &Win_GParted::activate_mount_partition), t ) ) );

				dynamic_cast<Gtk::Label*>( menu ->items() .back() .get_child() ) ->set_use_underline( false ) ;
			}

			menu_partition .items()[ MENU_TOGGLE_BUSY ] .hide() ;
			menu_partition .items()[ MENU_MOUNT ] .show() ;	
		}

		// See if there is a partition to be copied and it fits inside this selected partition
		if ( copied_partition != NULL                                              &&
		     ( copied_partition->get_filesystem_partition().get_byte_length() <=
		       selected_filesystem.get_byte_length()                             ) &&
		     selected_partition_ptr->status == STAT_REAL                           &&
		     *copied_partition != *selected_partition_ptr                             )
			allow_paste( true );

		//see if we can somehow check/repair this file system....
		if ( selected_partition_ptr->status == STAT_REAL && fs_cap.check )
			allow_check( true ) ;
	}
}

void Win_GParted::show_operationslist()
{
	//Enable (or disable) Undo and Apply buttons
	allow_undo_clear_apply( operations .size() ) ;

	//Updates view of operations list and sensitivity of Undo and Apply buttons
	Refresh_Visual();

	if ( operations .size() == 1 ) //first operation, open operationslist
		open_operationslist() ;

	//FIXME:  A slight flicker may be introduced by this extra display refresh.
	//An extra display refresh seems to prevent the disk area visual disk from
	//  disappearing when enough operations are added to require a scrollbar
	//  (about 4 operations with default window size).
	//  Note that commenting out the code to
	//  "//make scrollwindow focus on the last operation in the list"
	//  in HBoxOperations::load_operations() prevents this problem from occurring as well.
	//  See also Win_GParted::activate_undo().
	drawingarea_visualdisk .queue_draw() ;
}

void Win_GParted::open_operationslist() 
{
	if ( ! OPERATIONSLIST_OPEN )
	{
		OPERATIONSLIST_OPEN = true ;
		hbox_operations .show() ;
	
		for ( int t = vpaned_main .get_height() ; t > ( vpaned_main .get_height() - 100 ) ; t -= 5 )
		{
			vpaned_main .set_position( t );
			while ( Gtk::Main::events_pending() ) 
				Gtk::Main::iteration() ;
		}

		static_cast<Gtk::CheckMenuItem *>( & menubar_main .items()[ 2 ] .get_submenu() ->items()[ 1 ] )
			->set_active( true ) ;
	}
}

void Win_GParted::close_operationslist() 
{
	if ( OPERATIONSLIST_OPEN )
	{
		OPERATIONSLIST_OPEN = false ;
		
		for ( int t = vpaned_main .get_position() ; t < vpaned_main .get_height() ; t += 5 )
		{
			vpaned_main .set_position( t ) ;
		
			while ( Gtk::Main::events_pending() )
				Gtk::Main::iteration();
		}
		
		hbox_operations .hide() ;

		static_cast<Gtk::CheckMenuItem *>( & menubar_main .items()[ 2 ] .get_submenu() ->items()[ 1 ] )
			->set_active( false ) ;
	}
}

void Win_GParted::clear_operationslist() 
{
	remove_operation( -1, true ) ;
	close_operationslist() ;

	Refresh_Visual() ;
}

void Win_GParted::combo_devices_changed()
{
	unsigned int old_current_device = current_device;
	//set new current device
	current_device = combo_devices .get_active_row_number() ;
	if ( current_device == (unsigned int) -1 )
		current_device = old_current_device;
	if ( current_device >= devices .size() )
		current_device = 0 ;
	set_title( String::ucompose( _("%1 - GParted"), devices[ current_device ] .get_path() ) );
	
	//refresh label_device_info
	Fill_Label_Device_Info();
	
	//rebuild visualdisk and treeview
	Refresh_Visual();
	
	//uodate radiobuttons..
	if ( menubar_main .items()[ 0 ] .get_submenu() ->items()[ 1 ] .get_submenu() )
		static_cast<Gtk::RadioMenuItem *>( 
			& menubar_main .items()[ 0 ] .get_submenu() ->items()[ 1 ] .get_submenu() ->
			items()[ current_device ] ) ->set_active( true ) ;
}

void Win_GParted::radio_devices_changed( unsigned int item ) 
{
	if ( static_cast<Gtk::RadioMenuItem *>( 
	     	& menubar_main .items()[ 0 ] .get_submenu() ->items()[ 1 ] .get_submenu() ->
		items()[ item ] ) ->get_active() )
	{
		combo_devices .set_active( item ) ;
	}
}

void Win_GParted::on_show()
{
	Gtk::Window::on_show() ;
	
	vpaned_main .set_position( vpaned_main .get_height() ) ;
	close_operationslist() ;

	// Register callback for as soon as the main window has been shown to perform the
	// first load of the disk device details.  Do it this way because the Gtk  main
	// loop doesn't seem to enable quit handling until on_show(), this function, has
	// drawn the main window for the first time and returned, and we want Close Window
	// [Alt-F4] to work during the initial load of the disk device details.
	g_idle_add( initial_device_refresh, this );
}

// Callback used to load the disk device details for the first time
gboolean Win_GParted::initial_device_refresh( gpointer data )
{
	Win_GParted *win_gparted = static_cast<Win_GParted *>( data );
	win_gparted->menu_gparted_refresh_devices();
	return false;  // one shot g_idle_add() callback
}

void Win_GParted::menu_gparted_refresh_devices()
{
	show_pulsebar( _("Scanning all devices...") ) ;
	gparted_core.set_devices( devices );
	hide_pulsebar();
	
	//check if current_device is still available (think about hotpluggable stuff like usbdevices)
	if ( current_device >= devices .size() )
		current_device = 0 ;

	//see if there are any pending operations on non-existent devices
	//NOTE that this isn't 100% foolproof since some stuff (e.g. sourcedevice of copy) may slip through.
	//but anyone who removes the sourcedevice before applying the operations gets what he/she deserves :-)
	//FIXME: this actually sucks ;) see if we can use STL predicates here..
	unsigned int i ;
	for ( unsigned int t = 0 ; t < operations .size() ; t++ )
	{
		for ( i = 0 ; i < devices .size() && devices[ i ] != operations[ t ] ->device ; i++ ) {}
		
		if ( i >= devices .size() )
			remove_operation( t-- ) ;
	}
		
	//if no devices were detected we disable some stuff and show a message in the statusbar
	if ( devices .empty() )
	{
		this ->set_title( _("GParted") );
		combo_devices .hide() ;
		
		menubar_main .items()[ 0 ] .get_submenu() ->items()[ 1 ] .set_sensitive( false ) ;
		menubar_main .items()[ 1 ] .set_sensitive( false ) ;
		menubar_main .items()[ 2 ] .set_sensitive( false ) ;
		menubar_main .items()[ 3 ] .set_sensitive( false ) ;
		menubar_main .items()[ 4 ] .set_sensitive( false ) ;
		toolbar_main .set_sensitive( false ) ;
		drawingarea_visualdisk .set_sensitive( false ) ;
		treeview_detail .set_sensitive( false ) ;

		Fill_Label_Device_Info( true ) ;
		
		drawingarea_visualdisk .clear() ;
		treeview_detail .clear() ;
		
		//hmzz, this is really paranoid, but i think it's the right thing to do ;)
		hbox_operations .clear() ;
		close_operationslist() ;
		remove_operation( -1, true ) ;
		
		statusbar .pop() ;
		statusbar .push( _( "No devices detected" ) );
	}
	else //at least one device detected
	{
		combo_devices .show() ;
		
		menubar_main .items()[ 0 ] .get_submenu() ->items()[ 1 ] .set_sensitive( true ) ;
		menubar_main .items()[ 1 ] .set_sensitive( true ) ;
		menubar_main .items()[ 2 ] .set_sensitive( true ) ;
		menubar_main .items()[ 3 ] .set_sensitive( true ) ;
		menubar_main .items()[ 4 ] .set_sensitive( true ) ;

		toolbar_main .set_sensitive( true ) ;
		drawingarea_visualdisk .set_sensitive( true ) ;
		treeview_detail .set_sensitive( true ) ;
		
		refresh_combo_devices() ;	
	}
}

void Win_GParted::menu_gparted_features()
{
	DialogFeatures dialog ;
	dialog .set_transient_for( *this ) ;
	
	dialog .load_filesystems( gparted_core .get_filesystems() ) ;
	while ( dialog .run() == Gtk::RESPONSE_OK )
	{
		// Button [Rescan For Supported Actions] pressed in the dialog.  Rescan
		// for available core and file system specific commands and update the
		// view accordingly in the dialog.
		GParted_Core::find_supported_core();
		gparted_core .find_supported_filesystems() ;
		dialog .load_filesystems( gparted_core .get_filesystems() ) ;

		//recreate format menu...
		menu_partition .items()[ MENU_FORMAT ] .remove_submenu() ;
		menu_partition .items()[ MENU_FORMAT ] .set_submenu( * create_format_menu() ) ;
		menu_partition .items()[ MENU_FORMAT ] .get_submenu() ->show_all_children() ;
	}
}

void Win_GParted::menu_gparted_quit()
{
	if ( Quit_Check_Operations() )
		this ->hide();
}

void Win_GParted::menu_view_harddisk_info()
{ 
	if ( static_cast<Gtk::CheckMenuItem *>( & menubar_main .items()[ 2 ] .get_submenu() ->items()[ 0 ] ) ->get_active() )
	{	//open harddisk information
		hpaned_main .get_child1() ->show() ;		
		for ( int t = hpaned_main .get_position() ; t < 250 ; t += 15 )
		{
			hpaned_main .set_position( t );
			while ( Gtk::Main::events_pending() )
				Gtk::Main::iteration();
		}
	}
	else 
	{ 	//close harddisk information
		for ( int t = hpaned_main .get_position() ;  t > 0 ; t -= 15 )
		{
			hpaned_main .set_position( t );
			while ( Gtk::Main::events_pending() )
				Gtk::Main::iteration();
		}
		hpaned_main .get_child1() ->hide() ;
	}
}

void Win_GParted::menu_view_operations()
{
	if ( static_cast<Gtk::CheckMenuItem *>( & menubar_main .items()[ 2 ] .get_submenu() ->items()[ 1 ] ) ->get_active() )
		open_operationslist() ;
	else 
		close_operationslist() ;
}

void Win_GParted::show_disklabel_unrecognized ( Glib::ustring device_name )
{
	//Display dialog box indicating that no partition table was found on the device
	Gtk::MessageDialog dialog( *this,
			/*TO TRANSLATORS: looks like   No partition table found on device /dev/sda */
			String::ucompose( _( "No partition table found on device %1" ), device_name ),
			false,
			Gtk::MESSAGE_INFO,
			Gtk::BUTTONS_OK,
			true ) ;
	Glib::ustring tmp_msg = _( "A partition table is required before partitions can be added." ) ;
	tmp_msg += "\n" ;
	tmp_msg += _( "To create a new partition table choose the menu item:" ) ;
	tmp_msg += "\n" ;
	/*TO TRANSLATORS: this message represents the menu item Create Partition Table under the Device menu. */
	tmp_msg += _( "Device --> Create Partition Table." ) ;
	dialog .set_secondary_text( tmp_msg ) ;
	dialog .run() ;
}

void Win_GParted::show_help_dialog( const Glib::ustring & filename /* E.g., gparted */
                                  , const Glib::ustring & link_id  /* For context sensitive help */
                                  )
{
	GError *error = NULL ;
	GdkScreen *gscreen = NULL ;

	Glib::ustring uri = "ghelp:" + filename ;
	if (link_id .size() > 0 ) {
		uri = uri + "?" + link_id ;
	}

	gscreen = gdk_screen_get_default() ;

#ifdef HAVE_GTK_SHOW_URI
	gtk_show_uri( gscreen, uri .c_str(), gtk_get_current_event_time(), &error ) ;
#else
	Glib::ustring command = "gnome-open " + uri ;
	gdk_spawn_command_line_on_screen( gscreen, command .c_str(), &error ) ;
#endif
	if ( error != NULL )
	{
		//Try opening yelp application directly
		g_clear_error( &error ) ;  //Clear error from trying to open gparted help manual above (gtk_show_uri or gnome-open).
		Glib::ustring command = "yelp " + uri ;
		gdk_spawn_command_line_on_screen( gscreen, command .c_str(), &error ) ;
	}

	if ( error != NULL )
	{
		Gtk::MessageDialog dialog( *this
		                         , _( "Unable to open GParted Manual help file" )
		                         , false
		                         , Gtk::MESSAGE_ERROR
		                         , Gtk::BUTTONS_OK
		                         , true
		                         ) ;
		dialog .set_secondary_text( error ->message ) ;
		dialog .run() ;
	}
}

void Win_GParted::menu_help_contents()
{
#ifdef ENABLE_HELP_DOC
	//GParted was built with help documentation
	show_help_dialog( "gparted", "" );
#else
	//GParted was built *without* help documentation using --disable-doc
	Gtk::MessageDialog dialog( *this,
			_( "Documentation is not available" ),
			false,
			Gtk::MESSAGE_INFO,
			Gtk::BUTTONS_OK,
			true ) ;
	Glib::ustring tmp_msg = _( "This build of gparted is configured without documentation." ) ;
	tmp_msg += "\n" ;
	tmp_msg += _( "Documentation is available at the project web site." ) ;
	tmp_msg += "\n" ;
	tmp_msg += "http://gparted.org" ;
	dialog .set_secondary_text( tmp_msg ) ;
	dialog .run() ;
#endif
}

void Win_GParted::menu_help_about()
{
	std::vector<Glib::ustring> strings ;
	
	Gtk::AboutDialog dialog ;
	dialog .set_transient_for( *this ) ;
	
	dialog .set_name( _("GParted") ) ;
	dialog .set_logo_icon_name( "gparted" ) ;
	dialog .set_version( VERSION ) ;
	dialog .set_comments( _( "GNOME Partition Editor" ) ) ;
	std::string names ;
	names =    "Copyright © 2004-2006 Bart Hakvoort" ;
	names += "\nCopyright © 2008-2017 Curtis Gedak" ;
	names += "\nCopyright © 2011-2017 Mike Fleetwood" ;
	dialog .set_copyright( names ) ;

	//authors
	//Names listed in alphabetical order by LAST name.
	//See also AUTHORS file -- names listed in opposite order to try to be fair.
	strings .push_back( "Sinlu Bes <e80f00@gmail.com>" ) ;
	strings .push_back( "Luca Bruno <lucab@debian.org>" ) ;
	strings .push_back( "Wrolf Courtney <wrolf@wrolf.net>" ) ;
	strings .push_back( "Jérôme Dumesnil <jerome.dumesnil@gmail.com>" ) ;
	strings .push_back( "Markus Elfring <elfring@users.sourceforge.net>" ) ;
	strings .push_back( "Mike Fleetwood <mike.fleetwood@googlemail.com>" ) ;
	strings .push_back( "Curtis Gedak <gedakc@users.sf.net>" ) ;
	strings .push_back( "Matthias Gehre <m.gehre@gmx.de>" ) ;
	strings .push_back( "Rogier Goossens <goossens.rogier@gmail.com>" ) ;
	strings .push_back( "Bart Hakvoort <gparted@users.sf.net>" ) ;
	strings .push_back( "Seth Heeren <sgheeren@gmail.com>" ) ;
	strings .push_back( "Joan Lledó <joanlluislledo@gmail.com>" ) ;
	strings .push_back( "Phillip Susi <psusi@cfl.rr.com>" ) ;
	strings. push_back( "Michael Zimmermann <sigmaepsilon92@gmail.com>" ) ;
	dialog .set_authors( strings ) ;
	strings .clear() ;

	//artists
	strings .push_back( "Sebastian Kraft <kraft.sebastian@gmail.com>" ) ;
	dialog .set_artists( strings ) ;
	strings .clear() ;

	/*TO TRANSLATORS: your name(s) here please, if there are more translators put newlines (\n) between the names.
	  It's a good idea to provide the url of your translation team as well. Thanks! */
	Glib::ustring str_credits = _("translator-credits") ;
	if ( str_credits != "translator-credits" )
		dialog .set_translator_credits( str_credits ) ;


	//the url is not clickable - should not invoke web browser as root
	dialog .set_website_label( "http://gparted.org" ) ;

	dialog .run() ;
}

void Win_GParted::on_partition_selected( const Partition * partition_ptr, bool src_is_treeview )
{
	selected_partition_ptr = partition_ptr;

	set_valid_operations() ;

	if ( src_is_treeview )
		drawingarea_visualdisk.set_selected( partition_ptr );
	else
		treeview_detail.set_selected( partition_ptr );
}

void Win_GParted::on_partition_activated() 
{
	activate_info() ;
}

void Win_GParted::on_partition_popup_menu( unsigned int button, unsigned int time ) 
{
	menu_partition .popup( button, time );
}

bool Win_GParted::max_amount_prim_reached() 
{
	int primary_count = 0;
	for ( unsigned int i = 0 ; i < display_partitions.size() ; i ++ )
	{
		if ( display_partitions[i].type == TYPE_PRIMARY || display_partitions[i].type == TYPE_EXTENDED )
			primary_count ++;
	}

	//Display error if user tries to create more primary partitions than the partition table can hold. 
	if ( ! selected_partition_ptr->inside_extended && primary_count >= devices[current_device].max_prims )
	{
		Gtk::MessageDialog dialog( 
			*this,
			String::ucompose( ngettext( "It is not possible to create more than %1 primary partition"
			                          , "It is not possible to create more than %1 primary partitions"
			                          , devices[ current_device ] .max_prims
			                          )
			                , devices[ current_device ] .max_prims
			                ),
			false,
			Gtk::MESSAGE_ERROR,
			Gtk::BUTTONS_OK,
			true ) ;
		
		dialog .set_secondary_text(
			_( "If you want more partitions you should first create an extended partition. Such a partition can contain other partitions. Because an extended partition is also a primary partition it might be necessary to remove a primary partition first.") ) ;
		
		dialog .run() ;
		
		return true ;
	}
	
	return false ;
}

void Win_GParted::activate_resize()
{
	g_assert( selected_partition_ptr != NULL );  // Bug: Partition callback without a selected partition
	g_assert( valid_display_partition_ptr( selected_partition_ptr ) );  // Bug: Not pointing at a valid display partition object

	PartitionVector * display_partitions_ptr = &display_partitions;
	if ( selected_partition_ptr->type == TYPE_LOGICAL )
	{
		int index_extended = find_extended_partition( display_partitions );
		if ( index_extended >= 0 )
			display_partitions_ptr = &display_partitions[index_extended].logicals;
	}

	FS fs_cap = gparted_core.get_fs( selected_partition_ptr->get_filesystem_partition().filesystem );
	Partition * working_ptn;
	if ( selected_partition_ptr->filesystem == FS_LUKS && selected_partition_ptr->busy )
	{
		const FS & enc_cap = gparted_core.get_fs( FS_LUKS );

		// For an open LUKS mapping containing a file system being resized/moved
		// create a plain Partition object with the equivalent usage for the
		// Resize/Move dialog to work with.
		working_ptn = static_cast<const PartitionLUKS *>( selected_partition_ptr )->clone_as_plain();

		// Construct common capabilities from the file system ones.
		// Open LUKS encryption mapping can't be moved.
		fs_cap.move = FS::NONE;
		// Mask out resizing not also supported by open LUKS mapping.
		if ( ! enc_cap.online_grow )
		{
			fs_cap.grow        = FS::NONE;
			fs_cap.online_grow = FS::NONE;
		}
		if ( ! enc_cap.online_shrink )
		{
			fs_cap.shrink        = FS::NONE;
			fs_cap.online_shrink = FS::NONE;
		}
		// Adjust file system size limits accounting for LUKS encryption overhead.
		Sector luks_header_size = static_cast<const PartitionLUKS *>( selected_partition_ptr )->get_header_size();
		fs_cap.MIN = luks_header_size * working_ptn->sector_size +
		             ( fs_cap.MIN < MEBIBYTE ) ? MEBIBYTE : fs_cap.MIN;
		if ( fs_cap.MAX > 0 )
			fs_cap.MAX += luks_header_size * working_ptn->sector_size;
	}
	else
	{
		working_ptn = selected_partition_ptr->clone();
	}

	Dialog_Partition_Resize_Move dialog( fs_cap, *working_ptn, *display_partitions_ptr );
	dialog .set_transient_for( *this ) ;	

	delete working_ptn;
	working_ptn = NULL;

	if ( dialog .run() == Gtk::RESPONSE_OK )
	{
		dialog .hide() ;

		// Apply resize/move from the dialog to a copy of the selected partition.
		Partition * resized_ptn = selected_partition_ptr->clone();
		resized_ptn->resize( dialog.Get_New_Partition() );

		// When resizing/moving a partition which already exists on the disk all
		// possible operations could be pending so only try merging with the
		// previous operation.
		MergeType mergetype = MERGE_LAST_WITH_PREV;

		// If selected partition is NEW we simply remove the NEW operation from the list and add
		// it again with the new size and position ( unless it's an EXTENDED )
		if ( selected_partition_ptr->status == STAT_NEW && selected_partition_ptr->type != TYPE_EXTENDED )
		{
			resized_ptn->status = STAT_NEW;
			// On a partition which is pending creation only resize/move and
			// format operations are possible.  These operations are always
			// mergeable with the pending operation which will create the
			// partition.  Hence merge with any earlier operations to achieve
			// this.
			mergetype = MERGE_LAST_WITH_ANY;
		}

		Operation * operation = new OperationResizeMove( devices[current_device],
		                                                 *selected_partition_ptr,
		                                                 *resized_ptn );
		operation->icon = render_icon( Gtk::Stock::GOTO_LAST, Gtk::ICON_SIZE_MENU );

		delete resized_ptn;
		resized_ptn = NULL;

		// Display warning if moving a non-extended partition which already exists
		// on the disk.
		if ( operation->get_partition_original().status       != STAT_NEW                                    &&
		     operation->get_partition_original().sector_start != operation->get_partition_new().sector_start &&
		     operation->get_partition_original().type         != TYPE_EXTENDED                                  )
		{
			// Warn that move operation might break boot process
			Gtk::MessageDialog dialog( *this,
			                           _("Moving a partition might cause your operating system to fail to boot"),
			                           false,
			                           Gtk::MESSAGE_WARNING,
			                           Gtk::BUTTONS_OK,
			                           true );
			Glib::ustring tmp_msg =
					/*TO TRANSLATORS: looks like   You queued an operation to move the start sector of partition /dev/sda3. */
					String::ucompose( _( "You have queued an operation to move the start sector of partition %1." )
					                , operation->get_partition_original().get_path() );
			tmp_msg += _("  Failure to boot is most likely to occur if you move the GNU/Linux partition containing /boot, or if you move the Windows system partition C:.");
			tmp_msg += "\n";
			tmp_msg += _("You can learn how to repair the boot configuration in the GParted FAQ.");
			tmp_msg += "\n";
			tmp_msg += "http://gparted.org/faq.php";
			tmp_msg += "\n\n";
			tmp_msg += _("Moving a partition might take a very long time to apply.");
			dialog.set_secondary_text( tmp_msg );
			dialog.run();
		}

		Add_Operation( operation );
		merge_operations( mergetype );
	}

	show_operationslist() ;
}

void Win_GParted::activate_copy()
{
	g_assert( selected_partition_ptr != NULL );  // Bug: Partition callback without a selected partition
	g_assert( valid_display_partition_ptr( selected_partition_ptr ) );  // Bug: Not pointing at a valid display partition object

	delete copied_partition;
	copied_partition = selected_partition_ptr->clone();
}

void Win_GParted::activate_paste()
{
	g_assert( copied_partition != NULL );  // Bug: Paste called without partition to copy
	g_assert( selected_partition_ptr != NULL );  // Bug: Partition callback without a selected partition
	g_assert( valid_display_partition_ptr( selected_partition_ptr ) );  // Bug: Not pointing at a valid display partition object

	// Unrecognised whole disk device (See GParted_Core::get_devices_threads(), "unrecognized")
	if ( selected_partition_ptr->whole_device && selected_partition_ptr->type == TYPE_UNALLOCATED )
	{
		show_disklabel_unrecognized( devices [current_device ] .get_path() ) ;
		return ;
	}

	const Partition & copied_filesystem_ptn = copied_partition->get_filesystem_partition();

	if ( selected_partition_ptr->type == TYPE_UNALLOCATED )
	{
		if ( ! max_amount_prim_reached() )
		{
			// We don't want the messages, mount points or name of the source
			// partition for the new partition being created.
			Partition * part_temp = copied_filesystem_ptn.clone();
			part_temp->clear_messages();
			part_temp->clear_mountpoints();
			part_temp->name.clear();

			Dialog_Partition_Copy dialog( gparted_core.get_fs( copied_filesystem_ptn.filesystem ),
			                              *selected_partition_ptr,
			                              *part_temp );
			delete part_temp;
			part_temp = NULL;
			dialog .set_transient_for( *this );
		
			if ( dialog .run() == Gtk::RESPONSE_OK )
			{
				dialog .hide() ;

				Operation * operation = new OperationCopy( devices[current_device],
				                                           *selected_partition_ptr,
				                                           dialog.Get_New_Partition(),
				                                           *copied_partition );
				operation ->icon = render_icon( Gtk::Stock::COPY, Gtk::ICON_SIZE_MENU );

				// When pasting into unallocated space set a temporary
				// path of "Copy of /dev/SRC" for display purposes until
				// the partition is created and the real path queried.
				OperationCopy * copy_op = static_cast<OperationCopy*>( operation );
				copy_op->get_partition_new().set_path(
				        String::ucompose( _("Copy of %1"),
				                          copy_op->get_partition_copied().get_path() ) );

				Add_Operation( operation ) ;
			}
		}
	}
	else
	{
		const Partition & selected_filesystem_ptn = selected_partition_ptr->get_filesystem_partition();

		bool shown_dialog = false ;
		// VGNAME from mount mount
		if ( selected_filesystem_ptn.filesystem == FS_LVM2_PV   &&
		     ! selected_filesystem_ptn.get_mountpoint().empty()    )
		{
			if ( ! remove_non_empty_lvm2_pv_dialog( OPERATION_COPY ) )
				return ;
			shown_dialog = true ;
		}

		Partition * partition_new;
		if ( selected_partition_ptr->filesystem == FS_LUKS && ! selected_partition_ptr->busy )
		{
			// Pasting into a closed LUKS encrypted partition will overwrite
			// the encryption replacing it with a non-encrypted file system.
			// Start with a plain Partition object instead of cloning the
			// existing PartitionLUKS object containing a Partition object.
			// FIXME:
			// Expect to remove this case when creating and removing LUKS
			// encryption is added as a separate action when full LUKS
			// read-write support is implemented.
			// WARNING:
			// Deliberate object slicing of *selected_partition_ptr from
			// PartitionLUKS to Partition.
			partition_new = new Partition( *selected_partition_ptr );
		}
		else
		{
			// Pasting over a file system, either a plain file system or one
			// within an open LUKS encryption mapping.  Start with a clone of
			// the existing Partition object whether it be a plain Partition
			// object or a PartitionLUKS object containing a Partition object.
			partition_new = selected_partition_ptr->clone();
		}
		partition_new->alignment = ALIGN_STRICT;

		{
			// Sub-block so that filesystem_ptn_new reference goes out of
			// scope before partition_new pointer is deallocated.
			Partition & filesystem_ptn_new = partition_new->get_filesystem_partition();
			filesystem_ptn_new.filesystem = copied_filesystem_ptn.filesystem;
			filesystem_ptn_new.set_filesystem_label( copied_filesystem_ptn.get_filesystem_label() );
			filesystem_ptn_new.uuid = copied_filesystem_ptn.uuid;
			Sector new_size = filesystem_ptn_new.get_sector_length();
			if ( copied_filesystem_ptn.get_sector_length() == new_size )
			{
				// Pasting into same size existing partition, therefore
				// only block copy operation will be performed maintaining
				// the file system size.
				filesystem_ptn_new.set_sector_usage(
					copied_filesystem_ptn.sectors_used + copied_filesystem_ptn.sectors_unused,
					copied_filesystem_ptn.sectors_unused );
			}
			else
			{
				// Pasting into larger existing partition, therefore block
				// copy followed by file system grow operations (if
				// supported) will be performed making the file system
				// fill the partition.
				filesystem_ptn_new.set_sector_usage(
					new_size,
					new_size - copied_filesystem_ptn.sectors_used );
			}
			filesystem_ptn_new.clear_messages();
		}
 
		Operation * operation = new OperationCopy( devices[current_device],
		                                           *selected_partition_ptr,
		                                           *partition_new,
		                                           *copied_partition );
		operation ->icon = render_icon( Gtk::Stock::COPY, Gtk::ICON_SIZE_MENU );

		delete partition_new;
		partition_new = NULL;

		Add_Operation( operation ) ;

		if ( ! shown_dialog )
		{
			//Only warn that this paste operation will overwrite data in the existing
			//  partition if not already shown the remove non-empty LVM2 PV dialog.
			Gtk::MessageDialog dialog( *this
			                         , _( "You have pasted into an existing partition" )
			                         , false
			                         , Gtk::MESSAGE_WARNING
			                         , Gtk::BUTTONS_OK
			                         , true
			                         ) ;
			dialog .set_secondary_text(
					/*TO TRANSLATORS: looks like   The data in /dev/sda3 will be lost if you apply this operation. */
					String::ucompose( _( "The data in %1 will be lost if you apply this operation." ),
					selected_partition_ptr->get_path() ) );
			dialog .run() ;
		}
	}

	show_operationslist() ;
}

void Win_GParted::activate_new()
{
	g_assert( selected_partition_ptr != NULL );  // Bug: Partition callback without a selected partition
	g_assert( valid_display_partition_ptr( selected_partition_ptr ) );  // Bug: Not pointing at a valid display partition object

	// Unrecognised whole disk device (See GParted_Core::get_devices_threads(), "unrecognized")
	if ( selected_partition_ptr->whole_device && selected_partition_ptr->type == TYPE_UNALLOCATED )
	{
		show_disklabel_unrecognized( devices [current_device ] .get_path() ) ;
	}
	else if ( ! max_amount_prim_reached() )
	{
		// Check if an extended partition already exist; so that the dialog can
		// decide whether to allow the creation of the only extended partition
		// type or not.
		bool any_extended = ( find_extended_partition( display_partitions ) >= 0 );
		Dialog_Partition_New dialog( devices[current_device],
		                             *selected_partition_ptr,
		                             any_extended,
		                             new_count,
		                             gparted_core.get_filesystems() );
		dialog .set_transient_for( *this );
		
		if ( dialog .run() == Gtk::RESPONSE_OK )
		{
			dialog .hide() ;
			
			new_count++ ;
			Operation * operation = new OperationCreate( devices[current_device],
			                                             *selected_partition_ptr,
			                                             dialog.Get_New_Partition() );
			operation ->icon = render_icon( Gtk::Stock::NEW, Gtk::ICON_SIZE_MENU );

			Add_Operation( operation );

			show_operationslist() ;
		}
	}
}

void Win_GParted::activate_delete()
{ 
	g_assert( selected_partition_ptr != NULL );  // Bug: Partition callback without a selected partition
	g_assert( valid_display_partition_ptr( selected_partition_ptr ) );  // Bug: Not pointing at a valid display partition object

	// VGNAME from mount mount
	if ( selected_partition_ptr->filesystem == FS_LVM2_PV && ! selected_partition_ptr->get_mountpoint().empty() )
	{
		if ( ! remove_non_empty_lvm2_pv_dialog( OPERATION_DELETE ) )
			return ;
	}

	/* since logicals are *always* numbered from 5 to <last logical> there can be a shift
	 * in numbers after deletion.
	 * e.g. consider /dev/hda5 /dev/hda6 /dev/hda7. Now after removal of /dev/hda6,
	 * /dev/hda7 is renumbered to /dev/hda6
	 * the new situation is now /dev/hda5 /dev/hda6. If /dev/hda7 was mounted 
	 * the OS cannot find /dev/hda7 anymore and the results aren't that pretty.
	 * It seems best to check for this and prohibit deletion with some explanation to the user.*/
	 if ( selected_partition_ptr->type             == TYPE_LOGICAL                         &&
	      selected_partition_ptr->status           != STAT_NEW                             &&
	      selected_partition_ptr->partition_number <  devices[current_device].highest_busy    )
	{	
		Gtk::MessageDialog dialog( *this,
		                           String::ucompose( _("Unable to delete %1!"), selected_partition_ptr->get_path() ),
		                           false,
		                           Gtk::MESSAGE_ERROR,
		                           Gtk::BUTTONS_OK,
		                           true );

		dialog .set_secondary_text( 
			String::ucompose( _("Please unmount any logical partitions having a number higher than %1"),
					  selected_partition_ptr->partition_number ) );

		dialog .run() ;
		return;
	}
	
	//if partition is on the clipboard...(NOTE: we can't use Partition::== here..)
	if ( copied_partition != NULL && selected_partition_ptr->get_path() == copied_partition->get_path() )
	{
		Gtk::MessageDialog dialog( *this,
		                           String::ucompose( _("Are you sure you want to delete %1?"),
		                                             selected_partition_ptr->get_path() ),
		                           false,
		                           Gtk::MESSAGE_QUESTION,
		                           Gtk::BUTTONS_NONE,
		                           true );

		dialog .set_secondary_text( _("After deletion this partition is no longer available for copying.") ) ;
		
		/*TO TRANSLATORS: dialogtitle, looks like   Delete /dev/hda2 (ntfs, 2345 MiB) */
		dialog.set_title( String::ucompose( _("Delete %1 (%2, %3)"),
		                                    selected_partition_ptr->get_path(),
		                                    Utils::get_filesystem_string( selected_partition_ptr->filesystem ),
		                                    Utils::format_size( selected_partition_ptr->get_sector_length(), selected_partition_ptr->sector_size ) ) );
		dialog .add_button( Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL );
		dialog .add_button( Gtk::Stock::DELETE, Gtk::RESPONSE_OK );
	
		dialog .show_all_children() ;

		if ( dialog .run() != Gtk::RESPONSE_OK )
			return ;

		// Deleting partition on the clipboard.  Clear clipboard.
		delete copied_partition;
		copied_partition = NULL;
	}

	// If deleted one is NEW, it doesn't make sense to add it to the operationslist,
	// we erase its creation and possible modifications like resize etc.. from the operationslist.
	// Calling Refresh_Visual will wipe every memory of its existence ;-)
	if ( selected_partition_ptr->status == STAT_NEW )
	{
		//remove all operations done on this new partition (this includes creation)	
		for ( int t = 0 ; t < static_cast<int>( operations .size() ) ; t++ ) 
			if ( operations[t]->type                           != OPERATION_DELETE                   &&
			     operations[t]->get_partition_new().get_path() == selected_partition_ptr->get_path()    )
				remove_operation( t-- ) ;
				
		//determine lowest possible new_count
		new_count = 0 ; 
		for ( unsigned int t = 0 ; t < operations .size() ; t++ )
			if ( operations[t]->type                                 != OPERATION_DELETE &&
			     operations[t]->get_partition_new().status           == STAT_NEW         &&
			     operations[t]->get_partition_new().partition_number >  new_count           )
				new_count = operations[t]->get_partition_new().partition_number;
			
		new_count += 1 ;

		// After deleting all operations for the never applied partition creation,
		// try to merge all remaining adjacent operations to catch any which are
		// newly adjacent and can now be merged.  (Applies to resize/move and
		// format operations on real, already existing partitions which are only
		// merged when adjacent).
		merge_operations( MERGE_ALL_ADJACENT );

		Refresh_Visual(); 
				
		if ( ! operations .size() )
			close_operationslist() ;
	}
	else //deletion of a real partition...
	{
		Operation * operation = new OperationDelete( devices[ current_device ], *selected_partition_ptr );
		operation ->icon = render_icon( Gtk::Stock::DELETE, Gtk::ICON_SIZE_MENU ) ;

		Add_Operation( operation ) ;
	}

	show_operationslist() ;
}

void Win_GParted::activate_info()
{
	g_assert( selected_partition_ptr != NULL );  // Bug: Partition callback without a selected partition
	g_assert( valid_display_partition_ptr( selected_partition_ptr ) );  // Bug: Not pointing at a valid display partition object

	Dialog_Partition_Info dialog( *selected_partition_ptr );
	dialog .set_transient_for( *this );
	dialog .run();
}

void Win_GParted::activate_format( GParted::FILESYSTEM new_fs )
{
	g_assert( selected_partition_ptr != NULL );  // Bug: Partition callback without a selected partition
	g_assert( valid_display_partition_ptr( selected_partition_ptr ) );  // Bug: Not pointing at a valid display partition object

	const Partition & filesystem_ptn = selected_partition_ptr->get_filesystem_partition();

	// For non-empty LVM2 PV confirm overwrite before continuing.  VGNAME from mount mount.
	if ( filesystem_ptn.filesystem == FS_LVM2_PV && ! filesystem_ptn.get_mountpoint().empty() )
	{
		if ( ! remove_non_empty_lvm2_pv_dialog( OPERATION_FORMAT ) )
			return ;
	}

	// Generate minimum and maximum partition size limits for the new file system.
	FS fs_cap = gparted_core.get_fs( new_fs );
	bool encrypted = false;
	if ( selected_partition_ptr->filesystem == FS_LUKS && selected_partition_ptr->busy )
	{
		encrypted = true;
		Byte_Value encryption_overhead = selected_partition_ptr->get_byte_length() -
		                                 filesystem_ptn.get_byte_length();
		fs_cap.MIN += encryption_overhead;
		if ( fs_cap.MAX > 0 )
			fs_cap.MAX += encryption_overhead;
	}

	// Confirm partition is the right size to store the file system before continuing.
	if ( ( selected_partition_ptr->get_byte_length() < fs_cap.MIN )               ||
	     ( fs_cap.MAX && selected_partition_ptr->get_byte_length() > fs_cap.MAX )    )
	{
		Gtk::MessageDialog dialog( *this,
		                           String::ucompose( /* TO TRANSLATORS: looks like
		                                              * Cannot format this file system to fat16.
		                                              */
		                                             _("Cannot format this file system to %1"),
		                                             Utils::get_filesystem_string( encrypted, new_fs ) ),
		                           false,
		                           Gtk::MESSAGE_ERROR,
		                           Gtk::BUTTONS_OK,
		                           true );

		if ( selected_partition_ptr->get_byte_length() < fs_cap.MIN )
			dialog .set_secondary_text( String::ucompose(
						/* TO TRANSLATORS: looks like
						 * A fat16 file system requires a partition of at least 16.00 MiB.
						 */
						_( "A %1 file system requires a partition of at least %2."),
						Utils::get_filesystem_string( encrypted, new_fs ),
						Utils::format_size( fs_cap.MIN, 1 /* Byte */ ) ) );
		else
			dialog .set_secondary_text( String::ucompose(
						/* TO TRANSLATORS: looks like
						 * A partition with a hfs file system has a maximum size of 2.00 GiB.
						 */
						_( "A partition with a %1 file system has a maximum size of %2."),
						Utils::get_filesystem_string( encrypted, new_fs ),
						Utils::format_size( fs_cap.MAX, 1 /* Byte */ ) ) );
		
		dialog .run() ;
		return ;
	}

	// Compose Partition object to represent the format operation.
	Partition * temp_ptn;
	if ( selected_partition_ptr->filesystem == FS_LUKS && ! selected_partition_ptr->busy )
	{
		// Formatting a closed LUKS encrypted partition will erase the encryption
		// replacing it with a non-encrypted file system.  Start with a plain
		// Partition object instead of cloning the existing PartitionLUKS object
		// containing a Partition object.
		// FIXME:
		// Expect to remove this case when creating and removing LUKS encryption
		// is added as a separate action when full LUKS read-write support is
		// implemented.
		temp_ptn = new Partition;
	}
	else
	{
		// Formatting a file system, either a plain file system or one within an
		// open LUKS encryption mapping.  Start with a clone of the existing
		// Partition object whether it be a plain Partition object or a
		// PartitionLUKS object containing a Partition object.
		temp_ptn = selected_partition_ptr->clone();
	}
	{
		// Sub-block so that temp_filesystem_ptn reference goes out of scope
		// before temp_ptn pointer is deallocated.
		Partition & temp_filesystem_ptn = temp_ptn->get_filesystem_partition();
		temp_filesystem_ptn.Reset();
		temp_filesystem_ptn.Set( filesystem_ptn.device_path,
		                         filesystem_ptn.get_path(),
		                         filesystem_ptn.partition_number,
		                         filesystem_ptn.type,
		                         filesystem_ptn.whole_device,
		                         new_fs,
		                         filesystem_ptn.sector_start,
		                         filesystem_ptn.sector_end,
		                         filesystem_ptn.sector_size,
		                         filesystem_ptn.inside_extended,
		                         false );
		// Leave sector usage figures as new Partition object defaults of
		// -1, -1, 0 (_used, _unused, _unallocated) representing unknown.
	}
	temp_ptn->name = selected_partition_ptr->name;
	temp_ptn->status = STAT_FORMATTED;

	// When formatting a partition which already exists on the disk, all possible
	// operations could be pending so only try merging with the previous operation.
	MergeType mergetype = MERGE_LAST_WITH_PREV;

	// If selected partition is NEW we simply remove the NEW operation from the list and
	// add it again with the new file system
	if ( selected_partition_ptr->status == STAT_NEW )
	{
		temp_ptn->status = STAT_NEW;
		// On a partition which is pending creation only resize/move and format
		// operations are possible.  These operations are always mergeable with
		// the pending operation which will create the partition.  Hence merge
		// with any earlier operations to achieve this.
		mergetype = MERGE_LAST_WITH_ANY;
	}

	Operation * operation = new OperationFormat( devices[current_device],
	                                             *selected_partition_ptr,
	                                             *temp_ptn );
	operation->icon = render_icon( Gtk::Stock::CONVERT, Gtk::ICON_SIZE_MENU );

	delete temp_ptn;
	temp_ptn = NULL;

	Add_Operation( operation );
	merge_operations( mergetype );

	show_operationslist() ;
}

bool Win_GParted::unmount_partition( const Partition & partition, Glib::ustring & error )
{
	const std::vector<Glib::ustring> fs_mountpoints = partition.get_mountpoints();
	const std::vector<Glib::ustring> all_mountpoints = Mount_Info::get_all_mountpoints();

	std::vector<Glib::ustring> skipped_mountpoints;
	std::vector<Glib::ustring> umount_errors;

	for ( unsigned int i = 0 ; i < fs_mountpoints.size() ; i ++ )
	{
		if ( std::count( all_mountpoints.begin(), all_mountpoints.end(), fs_mountpoints[i] ) >= 2 )
		{
			// This mount point has two or more different file systems mounted
			// on top of each other.  Refuse to unmount it as in the general
			// case all have to be unmounted and then all except the file
			// system being unmounted have to be remounted.  This is too rare
			// a case to write such complicated code for.
			skipped_mountpoints.push_back( fs_mountpoints[i] );
		}
		else
		{
			Glib::ustring cmd = "umount -v \"" + fs_mountpoints[i] + "\"";
			Glib::ustring dummy;
			Glib::ustring umount_error;
			if ( Utils::execute_command( cmd, dummy, umount_error ) )
				umount_errors.push_back( "# " + cmd + "\n" + umount_error );
		}
	}

	if ( umount_errors.size() > 0 )
	{
		error = "<i>" + Glib::build_path( "</i>\n<i>", umount_errors ) + "</i>";
		return false;
	}
	if ( skipped_mountpoints.size() > 0 )
	{
		error = _("The partition could not be unmounted from the following mount points:");
		error += "\n\n<i>" + Glib::build_path( "\n", skipped_mountpoints ) + "</i>\n\n";
		error += _("This is because other partitions are also mounted on these mount points.  You are advised to unmount them manually.");
		return false;
	}
	return true;
}

bool Win_GParted::check_toggle_busy_allowed( const Glib::ustring & disallowed_msg )
{
	int operation_count = partition_in_operation_queue_count( *selected_partition_ptr );
	if ( operation_count > 0 )
	{
		Glib::ustring primary_msg = String::ucompose(
			/* TO TRANSLATORS: Singular case looks like   1 operation is currently pending for partition /dev/sdb1 */
			ngettext( "%1 operation is currently pending for partition %2",
			/* TO TRANSLATORS: Plural case looks like   3 operations are currently pending for partition /dev/sdb1 */
			          "%1 operations are currently pending for partition %2",
			          operation_count ),
			operation_count,
			selected_partition_ptr->get_path() );

		Gtk::MessageDialog dialog( *this,
		                           primary_msg,
		                           false,
		                           Gtk::MESSAGE_INFO,
		                           Gtk::BUTTONS_OK,
		                           true );

		Glib::ustring secondary_msg = disallowed_msg + "\n" +
			_("Use the Edit menu to undo, clear or apply pending operations.");
		dialog.set_secondary_text( secondary_msg );
		dialog.run();
		return false;
	}
	return true;
}

void Win_GParted::show_toggle_failure_dialog( const Glib::ustring & failure_summary,
                                              const Glib::ustring & marked_up_error )
{
	Gtk::MessageDialog dialog( *this,
	                           failure_summary,
	                           false,
	                           Gtk::MESSAGE_ERROR,
	                           Gtk::BUTTONS_OK,
	                           true );
	dialog.set_secondary_text( marked_up_error, true );
	dialog.run();
}

void Win_GParted::toggle_busy_state()
{
	g_assert( selected_partition_ptr != NULL );  // Bug: Partition callback without a selected partition
	g_assert( valid_display_partition_ptr( selected_partition_ptr ) );  // Bug: Not pointing at a valid display partition object

	enum Action
	{
		NONE          = 0,
		SWAPOFF       = 1,
		SWAPON        = 2,
		DEACTIVATE_VG = 3,
		ACTIVATE_VG   = 4,
		UNMOUNT       = 5
	};
	Action action = NONE;
	Glib::ustring disallowed_msg;
	Glib::ustring pulse_msg;
	Glib::ustring failure_msg;
	const Partition & filesystem_ptn = selected_partition_ptr->get_filesystem_partition();
	if ( filesystem_ptn.filesystem == FS_LINUX_SWAP && filesystem_ptn.busy )
	{
		action = SWAPOFF;
		disallowed_msg = _("The swapoff action cannot be performed when there are operations pending for the partition.");
		pulse_msg = String::ucompose( _("Deactivating swap on %1"), filesystem_ptn.get_path() );
		failure_msg = _("Could not deactivate swap");
	}
	else if ( filesystem_ptn.filesystem == FS_LINUX_SWAP && ! filesystem_ptn.busy )
	{
		action = SWAPON;
		disallowed_msg = _("The swapon action cannot be performed when there are operations pending for the partition.");
		pulse_msg = String::ucompose( _("Activating swap on %1"), filesystem_ptn.get_path() );
		failure_msg = _("Could not activate swap");
	}
	else if ( filesystem_ptn.filesystem == FS_LVM2_PV && filesystem_ptn.busy )
	{
		action = DEACTIVATE_VG;
		disallowed_msg = _("The deactivate Volume Group action cannot be performed when there are operations pending for the partition.");
		pulse_msg = String::ucompose( _("Deactivating Volume Group %1"),
		                              filesystem_ptn.get_mountpoint() );  // VGNAME from point point
		failure_msg = _("Could not deactivate Volume Group");
	}
	else if ( filesystem_ptn.filesystem == FS_LVM2_PV && ! filesystem_ptn.busy )
	{
		action = ACTIVATE_VG;
		disallowed_msg = _("The activate Volume Group action cannot be performed when there are operations pending for the partition.");
		pulse_msg = String::ucompose( _("Activating Volume Group %1"),
		                              filesystem_ptn.get_mountpoint() );  // VGNAME from point point
		failure_msg = _("Could not activate Volume Group");
	}
	else if ( filesystem_ptn.busy )
	{
		action = UNMOUNT;
		disallowed_msg = _("The unmount action cannot be performed when there are operations pending for the partition.");
		pulse_msg = String::ucompose( _("Unmounting %1"), filesystem_ptn.get_path() );
		failure_msg = String::ucompose( _("Could not unmount %1"), filesystem_ptn.get_path() );
	}
	else
		// Impossible.  Mounting a file system calls activate_mount_partition().
		return;

	if ( ! check_toggle_busy_allowed( disallowed_msg ) )
		// One or more operations are pending for this partition so changing the
		// busy state is not allowed.
		//
		// After set_valid_operations() has allowed the operations to be queued
		// the rest of the code assumes the busy state of the partition won't
		// change.  Therefore pending operations must prevent changing the busy
		// state of the partition.
		return;

	show_pulsebar( pulse_msg );
	bool success = false;
	Glib::ustring cmd;
	Glib::ustring output;
	Glib::ustring error;
	Glib::ustring error_msg;
	switch ( action )
	{
		case SWAPOFF:
			cmd = "swapoff -v " + filesystem_ptn.get_path();
			success = ! Utils::execute_command( cmd, output, error );
			error_msg = "<i># " + cmd + "\n" + error + "</i>";
			break;
		case SWAPON:
			cmd = "swapon -v " + filesystem_ptn.get_path();
			success = ! Utils::execute_command( cmd, output, error );
			error_msg = "<i># " + cmd + "\n" + error + "</i>";
			break;
		case DEACTIVATE_VG:
			cmd = "lvm vgchange -a n " + filesystem_ptn.get_mountpoint();
			success = ! Utils::execute_command( cmd, output, error );
			error_msg = "<i># " + cmd + "\n" + error + "</i>";
			break;
		case ACTIVATE_VG:
			cmd = "lvm vgchange -a y " + filesystem_ptn.get_mountpoint();
			success = ! Utils::execute_command( cmd, output, error );
			error_msg = "<i># " + cmd + "\n" + error + "</i>";
			break;
		case UNMOUNT:
			success = unmount_partition( filesystem_ptn, error_msg );
			break;
		default:
			// Impossible
			break;
	}
	hide_pulsebar();

	if ( ! success )
		show_toggle_failure_dialog( failure_msg, error_msg );

	menu_gparted_refresh_devices() ;
}

void Win_GParted::activate_mount_partition( unsigned int index ) 
{
	g_assert( selected_partition_ptr != NULL );  // Bug: Partition callback without a selected partition
	g_assert( valid_display_partition_ptr( selected_partition_ptr ) );  // Bug: Not pointing at a valid display partition object

	Glib::ustring disallowed_msg = _("The mount action cannot be performed when an operation is pending for the partition.");
	if ( ! check_toggle_busy_allowed( disallowed_msg ) )
		// One or more operations are pending for this partition so changing the
		// busy state by mounting the file system is not allowed.
		return;

	bool success;
	Glib::ustring cmd;
	Glib::ustring output;
	Glib::ustring error;
	Glib::ustring error_msg;

	const Partition & filesystem_ptn = selected_partition_ptr->get_filesystem_partition();
	show_pulsebar( String::ucompose( _("mounting %1 on %2"),
	                                 filesystem_ptn.get_path(),
	                                 filesystem_ptn.get_mountpoints()[index] ) );

	// First try mounting letting mount (libblkid) determine the file system type.
	cmd = "mount -v " + filesystem_ptn.get_path() +
	      " \"" + filesystem_ptn.get_mountpoints()[index] + "\"";
	success = ! Utils::execute_command( cmd, output, error );
	if ( ! success )
	{
		error_msg = "<i># " + cmd + "\n" + error + "</i>";

		Glib::ustring type = Utils::get_filesystem_kernel_name( filesystem_ptn.filesystem );
		if ( ! type.empty() )
		{
			// Second try mounting specifying the GParted determined file
			// system type.
			cmd = "mount -v -t " + type + " " + filesystem_ptn.get_path() +
			      " \"" + filesystem_ptn.get_mountpoints()[index] + "\"";
			success = ! Utils::execute_command( cmd, output, error );
			if ( ! success )
				error_msg += "\n<i># " + cmd + "\n" + error + "</i>";
		}
	}
	hide_pulsebar();
	if ( ! success )
	{
		Glib::ustring failure_msg = String::ucompose( _("Could not mount %1 on %2"),
		                                              filesystem_ptn.get_path(),
		                                              filesystem_ptn.get_mountpoints()[index] );
		show_toggle_failure_dialog( failure_msg, error_msg );
	}

	menu_gparted_refresh_devices() ;
}

void Win_GParted::activate_disklabel()
{
	//If there are active mounted partitions on the device then warn
	//  the user that all partitions must be unactive before creating
	//  a new partition table
	int active_count = active_partitions_on_device_count( devices[ current_device ] ) ;
	if ( active_count > 0 )
	{
		Glib::ustring tmp_msg =
		    String::ucompose( /*TO TRANSLATORS: Singular case looks like  1 partition is currently active on device /dev/sda */
		                      ngettext( "%1 partition is currently active on device %2"
		                      /*TO TRANSLATORS: Plural case looks like    3 partitions are currently active on device /dev/sda */
		                              , "%1 partitions are currently active on device %2"
		                              , active_count
		                              )
		                    , active_count
		                    , devices[ current_device ] .get_path()
		                    ) ;
		Gtk::MessageDialog dialog( *this
		                         , tmp_msg
		                         , false
		                         , Gtk::MESSAGE_INFO
		                         , Gtk::BUTTONS_OK
		                         , true
		                         ) ;
		tmp_msg  = _( "A new partition table cannot be created when there are active partitions." ) ;
		tmp_msg += "  " ;
		tmp_msg += _( "Active partitions are those that are in use, such as a mounted file system, or enabled swap space." ) ;
		tmp_msg += "\n" ;
		tmp_msg += _( "Use Partition menu options, such as unmount or swapoff, to deactivate all partitions on this device before creating a new partition table." ) ;
		dialog .set_secondary_text( tmp_msg ) ;
		dialog .run() ;
		return ;
	}

	//If there are pending operations then warn the user that these
	//  operations must either be applied or cleared before creating
	//  a new partition table.
	if ( operations .size() )
	{
		Glib::ustring tmp_msg =
		    String::ucompose( ngettext( "%1 operation is currently pending"
		                              , "%1 operations are currently pending"
		                              , operations .size()
		                              )
		                    , operations .size()
		                    ) ;
		Gtk::MessageDialog dialog( *this
		                         , tmp_msg
		                         , false
		                         , Gtk::MESSAGE_INFO
		                         , Gtk::BUTTONS_OK
		                         , true
		                         ) ;
		tmp_msg  = _( "A new partition table cannot be created when there are pending operations." ) ;
		tmp_msg += "\n" ;
		tmp_msg += _( "Use the Edit menu to either clear or apply all operations before creating a new partition table." ) ;
		dialog .set_secondary_text( tmp_msg ) ;
		dialog .run() ;
		return ;
	}

	//Display dialog for creating a new partition table.
	Dialog_Disklabel dialog( devices[ current_device ] ) ;
	dialog .set_transient_for( *this );

	if ( dialog .run() == Gtk::RESPONSE_APPLY )
	{
		if ( ! gparted_core.set_disklabel( devices[current_device], dialog.Get_Disklabel() ) )
		{
			Gtk::MessageDialog dialog( *this,
						   _("Error while creating partition table"),
						   true,
						   Gtk::MESSAGE_ERROR,
						   Gtk::BUTTONS_OK,
						   true ) ;
			dialog .run() ;
		}

		dialog .hide() ;
			
		menu_gparted_refresh_devices() ;
	}
}

//Runs when the Device->Attempt Rescue Data is clicked
void Win_GParted::activate_attempt_rescue_data()
{
	if(Glib::find_program_in_path( "gpart" ) .empty()) //Gpart must be installed to continue
	{
		Gtk::MessageDialog errorDialog(*this, "", true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
		errorDialog.set_message(_("Command gpart was not found"));
		errorDialog.set_secondary_text(_("This feature uses gpart. Please install gpart and try again."));

		errorDialog.run();

		return;
	}

	//Dialog information
	Glib::ustring sec_text = _( "A full disk scan is needed to find file systems." ) ;
	sec_text += "\n" ;
	sec_text +=_("The scan might take a very long time.");
	sec_text += "\n" ;
	sec_text += _("After the scan you can mount any discovered file systems and copy the data to other media.") ;
	sec_text += "\n" ;
	sec_text += _("Do you want to continue?");

	Gtk::MessageDialog messageDialog(*this, "", true, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK_CANCEL, true);
	/*TO TRANSLATORS: looks like	Search for file systems on /deb/sdb */
	messageDialog.set_message(String::ucompose(_("Search for file systems on %1"), devices[ current_device ] .get_path()));
	messageDialog.set_secondary_text(sec_text);

	if(messageDialog.run()!=Gtk::RESPONSE_OK)
	{
		return;
	}

	messageDialog.hide();

	/*TO TRANSLATORS: looks like	Searching for file systems on /deb/sdb */
	show_pulsebar(String::ucompose( _("Searching for file systems on %1"), devices[ current_device ] .get_path()));
	Glib::ustring gpart_output;
	gparted_core.guess_partition_table(devices[ current_device ], gpart_output);
	hide_pulsebar();
	Dialog_Rescue_Data dialog;
	dialog .set_transient_for( *this );

	//Reads the output of gpart
	dialog.init_partitions( &devices[current_device], gpart_output );

	if ( ! dialog.found_partitions() )
	{
		//Dialog information
		Gtk::MessageDialog errorDialog(*this, "", true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
		
		/*TO TRANSLATORS: looks like	No file systems found on /deb/sdb */
		errorDialog.set_message(String::ucompose(_("No file systems found on %1"), devices[ current_device ] .get_path()));
		errorDialog.set_secondary_text(_("The disk scan by gpart did not find any recognizable file systems on this disk."));

		errorDialog.run();
		return;
	}

	dialog.run();
	dialog.hide();

	Glib::ustring commandUmount= "umount /tmp/gparted-roview*";

	Utils::execute_command(commandUmount);

	menu_gparted_refresh_devices() ;
}

void Win_GParted::activate_manage_flags() 
{
	g_assert( selected_partition_ptr != NULL );  // Bug: Partition callback without a selected partition
	g_assert( valid_display_partition_ptr( selected_partition_ptr ) );  // Bug: Not pointing at a valid display partition object

	get_window() ->set_cursor( Gdk::Cursor( Gdk::WATCH ) ) ;
	while ( Gtk::Main::events_pending() )
		Gtk::Main::iteration() ;

	DialogManageFlags dialog( *selected_partition_ptr, gparted_core.get_available_flags( *selected_partition_ptr ) );
	dialog .set_transient_for( *this ) ;
	dialog .signal_get_flags .connect(
		sigc::mem_fun( &gparted_core, &GParted_Core::get_available_flags ) ) ;
	dialog .signal_toggle_flag .connect(
		sigc::mem_fun( &gparted_core, &GParted_Core::toggle_flag ) ) ;

	get_window() ->set_cursor() ;
	
	dialog .run() ;
	dialog .hide() ;
	
	if ( dialog .any_change )
		menu_gparted_refresh_devices() ;
}
	
void Win_GParted::activate_check() 
{
	g_assert( selected_partition_ptr != NULL );  // Bug: Partition callback without a selected partition
	g_assert( valid_display_partition_ptr( selected_partition_ptr ) );  // Bug: Not pointing at a valid display partition object

	// FIXME: Consider constructing new partition object with zero unallocated and
	// messages cleared to represent how applying a check operation also grows the
	// file system to fill the partition.
	Operation * operation = new OperationCheck( devices[current_device], *selected_partition_ptr );

	operation ->icon = render_icon( Gtk::Stock::EXECUTE, Gtk::ICON_SIZE_MENU );

	Add_Operation( operation ) ;
	// Try to merge this check operation with all previous operations.
	merge_operations( MERGE_LAST_WITH_ANY );

	show_operationslist() ;
}

void Win_GParted::activate_label_filesystem()
{
	g_assert( selected_partition_ptr != NULL );  // Bug: Partition callback without a selected partition
	g_assert( valid_display_partition_ptr( selected_partition_ptr ) );  // Bug: Not pointing at a valid display partition object

	const Partition & filesystem_ptn = selected_partition_ptr->get_filesystem_partition();
	Dialog_FileSystem_Label dialog( filesystem_ptn );
	dialog .set_transient_for( *this );

	if (	dialog .run() == Gtk::RESPONSE_OK
	     && dialog.get_new_label() != filesystem_ptn.get_filesystem_label() )
	{
		dialog .hide() ;
		// Make a duplicate of the selected partition (used in UNDO)
		Partition * part_temp = selected_partition_ptr->clone();

		part_temp->get_filesystem_partition().set_filesystem_label( dialog.get_new_label() );

		Operation * operation = new OperationLabelFileSystem( devices[current_device],
		                                                      *selected_partition_ptr, *part_temp );
		operation ->icon = render_icon( Gtk::Stock::EXECUTE, Gtk::ICON_SIZE_MENU );

		delete part_temp;
		part_temp = NULL;

		Add_Operation( operation ) ;
		// Try to merge this label file system operation with all previous
		// operations.
		merge_operations( MERGE_LAST_WITH_ANY );

		show_operationslist() ;
	}
}

void Win_GParted::activate_name_partition()
{
	g_assert( selected_partition_ptr != NULL );  // Bug: Partition callback without a selected partition
	g_assert( valid_display_partition_ptr( selected_partition_ptr ) );  // Bug: Not pointing at a valid display partition object

	Dialog_Partition_Name dialog( *selected_partition_ptr,
	                              devices[current_device].get_max_partition_name_length() );
	dialog.set_transient_for( *this );

	if (	dialog.run() == Gtk::RESPONSE_OK
	     && dialog.get_new_name() != selected_partition_ptr->name )
	{
		dialog.hide();
		// Make a duplicate of the selected partition (used in UNDO)
		Partition * part_temp = selected_partition_ptr->clone();

		part_temp->name = dialog.get_new_name();

		Operation * operation = new OperationNamePartition( devices[current_device],
		                                                    *selected_partition_ptr, *part_temp );
		operation->icon = render_icon( Gtk::Stock::EXECUTE, Gtk::ICON_SIZE_MENU );

		delete part_temp;
		part_temp = NULL;

		Add_Operation( operation );
		// Try to merge this name partition operation with all previous
		// operations.
		merge_operations( MERGE_LAST_WITH_ANY );

		show_operationslist();
	}
}

void Win_GParted::activate_change_uuid()
{
	g_assert( selected_partition_ptr != NULL );  // Bug: Partition callback without a selected partition
	g_assert( valid_display_partition_ptr( selected_partition_ptr ) );  // Bug: Not pointing at a valid display partition object

	const Partition & filesystem_ptn = selected_partition_ptr->get_filesystem_partition();
	const FileSystem * filesystem_object = gparted_core.get_filesystem_object( filesystem_ptn.filesystem );
	if ( filesystem_object->get_custom_text( CTEXT_CHANGE_UUID_WARNING ) != "" )
	{
		int i ;
		Gtk::MessageDialog dialog( *this,
		                           filesystem_object->get_custom_text( CTEXT_CHANGE_UUID_WARNING, 0 ),
		                           false,
		                           Gtk::MESSAGE_WARNING,
		                           Gtk::BUTTONS_OK,
		                           true );
		Glib::ustring tmp_msg = "" ;
		for ( i = 1 ; filesystem_object->get_custom_text( CTEXT_CHANGE_UUID_WARNING, i ) != "" ; i++ )
		{
			if ( i > 1 )
				tmp_msg += "\n\n" ;
			tmp_msg += filesystem_object->get_custom_text( CTEXT_CHANGE_UUID_WARNING, i );
		}
		dialog .set_secondary_text( tmp_msg ) ;
		dialog .run() ;
	}

	// Make a duplicate of the selected partition (used in UNDO)
	Partition * temp_ptn = selected_partition_ptr->clone();

	{
		// Sub-block so that temp_filesystem_ptn reference goes out of scope
		// before temp_ptn pointer is deallocated.
		Partition & temp_filesystem_ptn = temp_ptn->get_filesystem_partition();
		if ( temp_filesystem_ptn.filesystem == FS_NTFS )
			// Explicitly ask for half, so that the user will be aware of it
			// Also, keep this kind of policy out of the NTFS code.
			temp_filesystem_ptn.uuid = UUID_RANDOM_NTFS_HALF;
		else
			temp_filesystem_ptn.uuid = UUID_RANDOM;
	}

	Operation * operation = new OperationChangeUUID( devices[current_device],
	                                                 *selected_partition_ptr, *temp_ptn );
	operation ->icon = render_icon( Gtk::Stock::EXECUTE, Gtk::ICON_SIZE_MENU );

	delete temp_ptn;
	temp_ptn = NULL;

	Add_Operation( operation ) ;
	// Try to merge this change UUID operation with all previous operations.
	merge_operations( MERGE_LAST_WITH_ANY );

	show_operationslist() ;
}

void Win_GParted::activate_undo()
{
	//when undoing a creation it's safe to decrease the newcount by one
	if ( operations .back() ->type == OPERATION_CREATE )
		new_count-- ;

	remove_operation() ;		
	
	Refresh_Visual();
	
	if ( ! operations .size() )
		close_operationslist() ;

	//FIXME:  A slight flicker may be introduced by this extra display refresh.
	//An extra display refresh seems to prevent the disk area visual disk from
	//  disappearing when there enough operations to require a scrollbar
	//  (about 4+ operations with default window size) and a user clicks on undo.
	//  See also Win_GParted::Add_operation().
	drawingarea_visualdisk .queue_draw() ;
}

void Win_GParted::remove_operation( int index, bool remove_all ) 
{
	if ( remove_all )
	{
		for ( unsigned int t = 0 ; t < operations .size() ; t++ )
			delete operations[ t ] ;

		operations .clear() ;
	}
	else if ( index == -1  && operations .size() > 0 )
	{
		delete operations .back() ;
		operations .pop_back() ;
	}
	else if ( index > -1 && index < static_cast<int>( operations .size() ) )
	{
		delete operations[ index ] ;
		operations .erase( operations .begin() + index ) ;
	}
}

int Win_GParted::partition_in_operation_queue_count( const Partition & partition )
{
	int operation_count = 0 ;

	for ( unsigned int t = 0 ; t < operations .size() ; t++ )
	{
		if ( partition.get_path() == operations[t]->get_partition_original().get_path() )
			operation_count++ ;
	}

	return operation_count ;
}

int  Win_GParted::active_partitions_on_device_count( const Device & device )
{
	int active_count = 0 ;

	//Count the active partitions on the device
	for ( unsigned int k=0; k < device .partitions .size(); k++ )
	{
		// Count the active primary and extended partitions
		if (   device .partitions[ k ] .busy
		    && device .partitions[ k ] .type != TYPE_UNALLOCATED
		   )
			active_count++ ;

		//Count the active logical partitions
		if (   device .partitions[ k ] .busy
		    && device .partitions[ k ] .type == TYPE_EXTENDED
		   )
		{
			for ( unsigned int j=0; j < device .partitions[ k ] .logicals .size(); j++ )
			{
				if (   device .partitions[ k ] .logicals [ j ] .busy
				    && device .partitions[ k ] .logicals [ j ] .type != TYPE_UNALLOCATED
				   )
					active_count++ ;
			}
		}
	}

	return active_count ;
}

void Win_GParted::activate_apply()
{
	Gtk::MessageDialog dialog( *this,
				   _("Are you sure you want to apply the pending operations?"),
				   false,
				   Gtk::MESSAGE_WARNING,
				   Gtk::BUTTONS_NONE,
				   true );
	Glib::ustring temp;
	temp =  _( "Editing partitions has the potential to cause LOSS of DATA.") ;
	temp += "\n" ;
	temp += _( "You are advised to backup your data before proceeding." ) ;
	dialog .set_secondary_text( temp ) ;
	dialog .set_title( _( "Apply operations to device" ) );
	
	dialog .add_button( Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL );
	dialog .add_button( Gtk::Stock::APPLY, Gtk::RESPONSE_OK );
	
	dialog .show_all_children() ;
	if ( dialog.run() == Gtk::RESPONSE_OK )
	{
		dialog .hide() ; //hide confirmationdialog
		
		Dialog_Progress dialog_progress( operations ) ;
		dialog_progress .set_transient_for( *this ) ;
		dialog_progress .signal_apply_operation .connect(
			sigc::mem_fun(gparted_core, &GParted_Core::apply_operation_to_disk) ) ;
		dialog_progress .signal_get_libparted_version .connect(
			sigc::mem_fun(gparted_core, &GParted_Core::get_libparted_version) ) ;
 
		int response ;
		do
		{
			response = dialog_progress .run() ;
		}
		while ( response == Gtk::RESPONSE_CANCEL || response == Gtk::RESPONSE_OK ) ;
		
		dialog_progress .hide() ;
		
		//wipe operations...
		remove_operation( -1, true ) ;
		hbox_operations .clear() ;
		close_operationslist() ;
							
		//reset new_count to 1
		new_count = 1 ;
		
		//reread devices and their layouts...
		menu_gparted_refresh_devices() ;
	}
}

bool Win_GParted::remove_non_empty_lvm2_pv_dialog( const OperationType optype )
{
	Glib::ustring tmp_msg ;
	switch ( optype )
	{
		case OPERATION_DELETE:
			tmp_msg = String::ucompose( _( "You are deleting non-empty LVM2 Physical Volume %1" ),
			                            selected_partition_ptr->get_path() );
			break ;
		case OPERATION_FORMAT:
			tmp_msg = String::ucompose( _( "You are formatting over non-empty LVM2 Physical Volume %1" ),
			                            selected_partition_ptr->get_path() );
			break ;
		case OPERATION_COPY:
			tmp_msg = String::ucompose( _( "You are pasting over non-empty LVM2 Physical Volume %1" ),
			                            selected_partition_ptr->get_path() );
			break ;
		default:
			break ;
	}
	Gtk::MessageDialog dialog( *this, tmp_msg,
	                           false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_NONE, true ) ;

	tmp_msg =  _( "Deleting or overwriting the Physical Volume is irrecoverable and will destroy or damage the "
	              " Volume Group." ) ;
	tmp_msg += "\n\n" ;
	tmp_msg += _( "To avoid destroying or damaging the Volume Group, you are advised to cancel and use external "
	              "LVM commands to free the Physical Volume before attempting this operation." ) ;
	tmp_msg += "\n\n" ;
	tmp_msg += _( "Do you want to continue to forcibly delete the Physical Volume?" ) ;

	Glib::ustring vgname = LVM2_PV_Info::get_vg_name( selected_partition_ptr->get_path() );
	std::vector<Glib::ustring> members ;
	if ( ! vgname .empty() )
		members = LVM2_PV_Info::get_vg_members( vgname );

	//Single copy of each string for translation purposes
	const Glib::ustring vgname_label  = _( "Volume Group:" ) ;
	const Glib::ustring members_label = _( "Members:" ) ;

#ifndef HAVE_GET_MESSAGE_AREA
	//Basic method of displaying VG members by appending it to the secondary text in the dialog.
	tmp_msg += "\n____________________\n\n" ;
	tmp_msg += "<b>" ;
	tmp_msg +=       vgname_label ;
	tmp_msg +=                    "</b>    " ;
	tmp_msg +=                               vgname ;
	tmp_msg += "\n" ;
	tmp_msg += "<b>" ;
	tmp_msg +=       members_label ;
	tmp_msg +=                     "</b>" ;
	if ( ! members .empty() )
	{
		tmp_msg += "    " ;
		tmp_msg +=        members [0] ;
		for ( unsigned int i = 1 ; i < members .size() ; i ++ )
		{
			tmp_msg += "    " ;
			tmp_msg +=        members [i] ;
		}
	}
#endif /* ! HAVE_GET_MESSAGE_AREA */

	dialog .set_secondary_text( tmp_msg, true ) ;

#ifdef HAVE_GET_MESSAGE_AREA
	//Nicely formatted method of displaying VG members by using a table below the secondary text
	//  in the dialog.  Uses Gtk::MessageDialog::get_message_area() which was new in gtkmm-2.22
	//  released September 2010.
	Gtk::Box * msg_area = dialog .get_message_area() ;

	Gtk::HSeparator * hsep( manage( new Gtk::HSeparator() ) ) ;
	msg_area ->pack_start( * hsep ) ;

	Gtk::Table * table( manage( new Gtk::Table() ) ) ;
        table ->set_border_width( 0 ) ;
        table ->set_col_spacings( 10 ) ;
        msg_area ->pack_start( * table ) ;

	int top = 0, bottom = 1 ;

	//Volume Group
	table ->attach( * Utils::mk_label( "<b>" + Glib::ustring( vgname_label ) + "</b>" ),
	                0, 1, top, bottom, Gtk::FILL ) ;
	table ->attach( * Utils::mk_label( vgname, true, false, true ),
	                1, 2, top++, bottom++, Gtk::FILL ) ;

	//Members
	table ->attach( * Utils::mk_label( "<b>" + Glib::ustring( members_label ) + "</b>",
	                                   true, false, false, 0.0 /* ALIGN_TOP */ ),
	                0, 1, top, bottom, Gtk::FILL ) ;

	Glib::ustring members_str = "" ;
	if ( ! members .empty() )
	{
		for ( unsigned int i = 0 ; i < members .size() ; i ++ )
		{
			if ( i > 0 )
				members_str += "\n" ;
			members_str += members[i] ;
		}
	}
	table ->attach( * Utils::mk_label( members_str, true, false, true, 0.0 /* ALIGN_TOP */ ),
	                1, 2, top++, bottom++, Gtk::FILL ) ;
#endif /* HAVE_GET_MESSAGE_AREA */

	dialog .add_button( Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL );
	dialog .add_button( Gtk::Stock::DELETE, Gtk::RESPONSE_OK );
	dialog .show_all() ;
	if ( dialog .run() == Gtk::RESPONSE_OK )
		return true ;
	return false ;
}

} // GParted
