use G15Daemon;

use GD;
use GD::Graph::lines;
#use GD::Graph::bars;

# simple demo. creates two screens (selectable via L1 button) and 
# displays some text & a border on one, and a graph on the other..


@data = (
    ["1st","2nd","3rd","4th","5th","6th","7th", "8th", "9th"],
    [    1,    2,    5,    6,    3,  1.5,    1,     3,     4],
    [ sort { $a <=> $b } (1, 2, 5, 6, 3, 1.5, 1, 3, 4) ]
);

# possible screentypes are $g15pbuf (1bpp pixel buffer) 
# $g15txt (text - not implemented yet) 
# or $g15wbmp (WBMP pixelbuffer useful for GD)
#
# ok.. create a couple of new screens..
my $lcdintro = G15Daemon->new($g15wbmp);
my $lcdgraph = G15Daemon->new($g15wbmp);
                                           
printf "perl bindings version is %f\n",$lcdintro->VERSION;
printf "interface version is %f\n ",$lcdintro->apiversion;

my $width = $lcdintro->width();
my $height = $lcdintro->height();

# create a new GD image
$im = new GD::Image ($width, $height);

# allocate some colors - these are the only valid ones for the G15
$black = $im->colorAllocate (0, 0, 0);
$white = $im->colorAllocate (255, 255, 255);

# fill with black
$im->fill(1,1,$black);

# Draw an oval
$im->arc (80, 22, 95, 42, 0, 360, $white);
# fill it with white
$im->fill(90,32,$white);

# draw a rectangle
$im->rectangle (10, 10, 150, 33, $white);
# fill it
$im->fill(12,12,$white);
$im->fill(145,12,$white);
# draw some text onto the buffer
$im->string(gdSmallFont,30,15,"G15Daemon & GDlib",$black);

#convert the buffer to WBMP format..
my $wbmpdata = $im->wbmp ($white);
#send it to the daemon....
$lcdintro->send($wbmpdata);

# create a graph, and send it on the other connection.
my $graph = GD::Graph::lines->new(160, 44);
$graph->set(
#         x_label           => 'X Label',
#         y_label           => 'Y label',
#         title             => 'Some simple graph',
         y_max_value       => 8,
         y_tick_number     => 8,
         y_label_skip      => 5,
         x_label_skip      => 2,
         dclrs => [ qw(dblue) ]
) or die $my_graph->error;

my $gd = $graph->plot(\@data) or die $graph->error;                                                                                       

# again convert to WBMP format and send it...
my $graphwbmp = $gd->wbmp ($black);
$lcdgraph->send($graphwbmp);

sleep(60);

$lcd->closescreen();
$lcdgraph->closescreen();


