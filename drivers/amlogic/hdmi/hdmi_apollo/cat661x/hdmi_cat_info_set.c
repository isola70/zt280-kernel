
#include "hdmi_global.h"
#include "hdmi_cat_defstx.h"
#include "hdmi_vmode.h"
#include "hdmi_i2c.h"
#include "hdmi_cat_video.h"
#include "hdmi_cat_info_set.h"
    
#include "hdmi_debug.h"

{
	
	
	
		
		
		
	
	WriteBlockHDMITX_CAT(REG_AVIINFO_DB1, 5,
			     (unsigned char *)&pAVIInfoFrame->pktbyte.
			     AVI_DB[0]);
	
			      (unsigned char *)&pAVIInfoFrame->pktbyte.
			      AVI_DB[5]);
	
		
		
		
	
	    0x80 + AVI_INFOFRAME_VER + AVI_INFOFRAME_TYPE + AVI_INFOFRAME_LEN;
	
	
	WriteByteHDMITX_CAT(REG_AVI_INFOFRM_CTRL, B_ENABLE_PKT | B_REPEAT_PKT);	// ENABLE_AVI_INFOFRM_PKT();
	


				     unsigned char *pAVIInfoFrame) 
{
	
//    if( !bEnable )
//    {
	    WriteByteHDMITX_CAT(REG_AVI_INFOFRM_CTRL, 0);	//disable PktAVI packet sending & repeat. bit0: enable/diable sending; bit1: enable/disble repeat
//    }
	
		
		
		


{
	
	
	
	    left_pixel_bar_end, right_pixel_bar_start;
	
	
	
	
		
	
		
			
			
			
			
		
		else
			
			
			
			
		
		
		
		
		
		break;
	
		
			
			
			
			
		
		else
			
			
			
			
		
		
		
		
		
		break;
	
		
			
			
			
			
		
		else
			
			
			
			
		
		
		
		
		
		break;
	
		
			
			
			
			
		
		else
			
			
			
			
		
		
		
		
		
		break;
	
		
		
		
		
		
		
		VIC = 4;
		
	
		
		
		
		
		
		
		VIC = 19;
		
	
		
		
		
		
		
		
		VIC = 5;
		
	
		
		
		
		
		
		
		VIC = 20;
		
	
		
		
		
		
		
		
		VIC = 16;
		
	
		
		
		
		
		
		
		VIC = 31;
		
	
		
		
	
		
	
		AviInfo.pktbyte.AVI_DB[0] = (2 << 5);
		
	
		AviInfo.pktbyte.AVI_DB[0] = (1 << 5);
		
	
	default:
		
		
		
	
	
//If the bar information and the active format information do not agree, then the bar information shall take precedence.    
	    AviInfo.pktbyte.AVI_DB[0] |= (0 << 2);	//bit[3:2]: Vert. and Horiz. Bar Info invalid,  added by chen
	
//A sink should adjust its scan based on S. Such a device would overscan(television) if it received S=1, where some active pixelsand lines at the edges are not displayed.
//and underscan if it received S=2. where all active pixels&lines are displayed, withor without a border.
//If it receives S=O,then it should overscan for a CE format and underscan for an IT format.
// A sink should indicate its overscan/underscan behavior using a Video Capabilities Data Block    
	    AviInfo.pktbyte.AVI_DB[0] &= 0xfc;	//|=0x01;      //Overscanned,  
	
	
	
	
	
	
	
	
	    (unsigned char)((top_line_bar_end & 0xff00) >> 8);
	
	    (unsigned char)(buttom_line_bar_start & 0xff);
	
	    (unsigned char)((buttom_line_bar_start & 0xff00) >> 8);
	
	
	    (unsigned char)((left_pixel_bar_end & 0xff00) >> 8);
	
	    (unsigned char)(right_pixel_bar_start & 0xff);
	
	    (unsigned char)((right_pixel_bar_start & 0xff00) >> 8);
	


////////////////////////////////////////////////////////////////////////////////
// Function: SetAudioInfoFrame()
// Parameter: pAudioInfoFrame - the pointer to HDMI Audio Infoframe ucData
// Return: N/A
// Remark: Fill the Audio InfoFrame ucData, and count checksum, then fill into
//         Audio InfoFrame registers.
// Side-Effect: N/A
////////////////////////////////////////////////////////////////////////////////

{
	
	
	
		
		
		
	
	
			     pAudioInfoFrame->pktbyte.AUD_DB[0]);
	
			     pAudioInfoFrame->pktbyte.AUD_DB[1]);
	
			     pAudioInfoFrame->pktbyte.AUD_DB[4]);
	
		
		
		
	
	    0x80 + AUDIO_INFOFRAME_VER + AUDIO_INFOFRAME_TYPE +
	    AUDIO_INFOFRAME_LEN;
	
	
	
	


					unsigned char *pAudioInfoFrame) 
{
	
//    if( !bEnable )
//    {
	    WriteByteHDMITX_CAT(REG_AUD_INFOFRM_CTRL, 0);	//disable PktAUO packet sending & repeat. bit0: enable/diable sending; bit1: enable/disble repeat
//    }
	if (bEnable)
		
		
		



////////////////////////////////////////////////////////////////////////////////
// Function: ConfigAudioInfoFrm
// Parameter: NumChannel, number from 1 to 8
// Remark: Evaluate. The speakerplacement is only for reference.
//         For production, the caller of SetAudioInfoFrame should program
//         Speaker placement by actual status.
//In cases where the audio information in the Audio InfoFrame does not agree with the actual 
//audio stream being received, the conflicting information in the Audio InfoFrame shall be ignored.
////////////////////////////////////////////////////////////////////////////////
void CAT_ConfigAudioInfoFrm(Hdmi_info_para_t * info) 
{
	
	
	
	
	
	
//HDMI requires the CT, SS and SF fields to be set to 0 ("Refer to Stream Header") as these items are carried in the audio stream.    
	    ct = 0;		//audio code type: Refer to Stream Header
	cc = 0;			//audio channel count: Refer to Stream Header
	ss = 0;			//sample size: Refer to Stream Header
	sf = 0;			//sampling frequency: Refer to Stream Header
	ca = 0;			//Speaker Mapping: 2 channels(FR & FL) ,applyonlyto multi-channel(Le.,morethantwochannels)uncompressedaudio.
	lsv = 0;		//DTV Monitor how much the source device attenuated the audio during a down-mixing operation. 0db  
	
//    // 6611 did not accept this, cannot set anything.
	    AudioInfo.pktbyte.AUD_DB[0] = (ct << 4) | cc;
	
	
	
	
	
		
		
		}
	
//    /* 
//    CAT6611 does not need to fill the sample size information in audio info frame.
//    ignore the part.
//    */
	    CAT_EnableAudioInfoFrame(1, (unsigned char *)&AudioInfo);
