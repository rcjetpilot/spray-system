#include "LPC17xx.h"
#include "../app/cfg/cfg_handler.h"
#include "../drivers/drivers.h"
#include "sbl_iap.h"
#include "math.h"
uint16_t Debug_Voltage_Press;
uint8_t pipe_status = 0;
uint16_t press0_value,press1_value;
uint16_t ADC_channel0_buf[50],ADC_channel1_buf[50];
uint16_t last_press0_value,last_press1_value;
uint16_t press0_value_min = 0,press1_value_min = 0;
extern int16_t idx_7_freq; // for test ������Ҫ�Ż�
float Voltage_Press_0 = 0;
float Voltage_Press_1 = 0;
void press0_cali_para_reset(void);
void press1_cali_para_reset(void);
spray_system_flow_cali_para_t spray_system_flow_cali_para1 __attribute__((aligned(4))); //�����Ʋ���
spray_system_pressure_cali_para_t spray_system_pressure_cali_para_1 __attribute__((aligned(4))); // 1 �űò���
spray_system_pressure_cali_para_t spray_system_pressure_cali_para_2 __attribute__((aligned(4))); // 2 �űò���
/************************************Һλ�������ݴ���**************************************************************/
uint8_t capacity_flag_1L = 1, level_stauts = 0;
 
uint8_t get_capacity_flag_1L(void)
{
  return capacity_flag_1L;
}

void level_sensor_init(void)
{
  LPC_GPIO1->FIODIR  &= (~(1<<31));// P1_31 Ϊ���� resaved0
}

uint16_t level_count;
void level_data_process(void)
{
  if(get_level_status_by_radar() == 1)
	 level_stauts = 0;
	else
   level_stauts = 1;
	
	if(level_stauts == 1)
	{
	   level_count++;
		 if(level_count >= 400)//4S
		 {
			  level_count = 400;
			  capacity_flag_1L = 1;
		 }
	}
	else
	{
	  level_count = 0;
		capacity_flag_1L = 0;
	}
}


/************************************�¶ȴ��������ݴ���*************************************************************/
float get_temperature(uint16_t tempe_sensor_value)
{
  float tempe;
	tempe = (1035 - tempe_sensor_value)/5.5;
  return tempe;
}																	

/************************************�������������ݴ���*************************************************************/
extern int16_t flow_sensor_value; // for test ������Ҫ�Ż�
float Volume_Each_Pulse = 0,user_volume = 0;
float flow_compute = 0; 
float current_capacity_part = 0;
//uint8_t Target_Frequency_Pulse1,Target_Frequency_Pulse2;
uint8_t Med_Frequency_Pulse;
uint16_t current_capacity_total = 0;
float Cali_Freq ; // У׼��ʱ��ƽ��������
float debug_watch_k_value = 0;
float Viscosity_Offset_Coeff = 0.05;// ճ�Ȳ�ϵ��
#define P_FREQ       0.6
#define Reference_K  0.49  // ��ˮ�������Ĭ��Kֵ
// ����ճ�ȵĲ�ͬ�Բ�ͬ�����µ�Kֵ���в���
//float debug_smm;
float Viscosity_Offset = 0,K_offset = 0;
float K_Coefficient_Compensation(void) // ������Kֵϵ������
{

// vk04  �궨����  0.9L/min---1.2L/min      ճ�Ȳ��ϵ��               ��ˮKֵ 0.5 
// 11001 �궨����  1.4L/min---1.8L/min      ճ�Ȳ��ϵ��  0.5
// vk08 �궨����   1.95L/min---2.2L/min     ճ�Ȳ��ϵ��  0.8
// ��궨Ƶ�ʵ������ ��ʾ���ٱ仯
// �����չ�ʽΪ  ����Ƶ��*K(1+ճ�Ȳ�*0.5*(�궨Ƶ�� - ʵʱƵ��))

     if((Volume_Each_Pulse) - Reference_K > 0)
		 {			 
				// �ж�Ϊճ��Һ��  ��ȥƫ��ֵ
				Viscosity_Offset  =	((Volume_Each_Pulse) - Reference_K)*Viscosity_Offset_Coeff;
		 }
		 else
		 {
		   Viscosity_Offset = 0; // �ж�Ϊ��ˮ
		 }
//	}
	
	
	if(get_cali_flow_freq() > 5)// && get_foc_throttle_value(FOC_1) > MIN_CNT && get_foc_throttle_value(FOC_2) > MIN_CNT
	{
		K_offset = Viscosity_Offset*(Cali_Freq - get_cali_flow_freq()); 
	}
	else
	{
	  K_offset = 0;
	}
	
	return K_offset;
}


uint16_t get_current_capacity_d(void)
{
	if(current_capacity_part > 25500)//16λ �����Ӵ� �ᴥ�����ֽ����
	 	current_capacity_part = 25500;
	
	current_capacity_total = current_capacity_part; 
  return current_capacity_total;
}


void Set_Volume_Each_Pulse(float value)
{
  Volume_Each_Pulse = value ; // + 0.03; // Ϊ�˷�ֹ����ҩ��������
}

void capacity_by_flow_sensor(void)// ֻ����һ���ط�����
{	
		if(get_cali_flow_freq() > 5) // ��ʱ������ʽ С�����Ƶ��Ĭ��Ϊ0
		{
			 debug_watch_k_value = Volume_Each_Pulse+K_Coefficient_Compensation();
			 flow_compute = get_cali_flow_freq()*Volume_Each_Pulse*(1+K_Coefficient_Compensation()); // ÿ�����ml
			 current_capacity_part += flow_compute; 
		}
}

/*********************************************************************************************************
*	�� �� ��:uint16_t MedFilter(*Value_buf)
*	����˵��:��ֵ�˲�
*	��    �Σ���
*	�� �� ֵ: uint16_t
*********************************************************************************************************/
#define N 51
uint16_t MedFilter(uint16_t *Value_buf)
{
	uint16_t temp=0;
	uint8_t i,j; 
	for (j=0;j<N-1;j++) 
	{ 
		for (i=0;i<N-j;i++) 
	  {    
			if ( Value_buf[i]>Value_buf[i+1] )
		  {        
				temp = Value_buf[i]; 
				Value_buf[i] = Value_buf[i+1]; 
				Value_buf[i+1] = temp;   
		  } 
	  } 
	}  
	return Value_buf[(N-1)/2];	
}
/*
*********************************************************************************************************
*	�� �� ��:temperature_sensor_handle
*	����˵��:�¶ȴ��������ݴ���
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/ 
uint16_t temperature0_value,temperature1_value;
float temperature0_voltage,temperature1_voltage;
float  temperature0_interface_battery,temperature1_interface_battery;
uint8_t XT100_battery_temp_state = State_Normal, XT90_battery_temp_state = State_Normal;
uint8_t Get_XT100_State(void)
{
	return XT100_battery_temp_state;
}

uint8_t Get_XT90_State(void)
{
	return XT90_battery_temp_state;
}
uint16_t temp0[51],temp1[51];//
void temperature_sensor_handle(void)
{
   
    static uint8_t cont=0;
	  static uint8_t  check_temp0_count_warning = 0,check_temp1_count_warning = 0;
	  static uint8_t  check_temp0_count_err = 0,check_temp1_count_err = 0;

    if(cont<51)
    {
        temp0[cont]=temperature0_sensor_get();
        temp1[cont]=temperature1_sensor_get(); 
        cont++;
    }
    else
    {
        cont=0;
        temperature0_value=MedFilter(temp0);
        temperature1_value=MedFilter(temp1);
        temperature0_voltage = ((float)temperature0_value /4096)*3.3f*1000;// 2/3,��λ��mv
        temperature1_voltage =  ((float)temperature1_value /4096)*3.3f*1000;// 2/3,��λ��mv
        temperature0_interface_battery  = get_temperature(temperature0_voltage);
    	  temperature1_interface_battery =  get_temperature(temperature1_voltage);
			
				if(temperature0_interface_battery<Temperature_Warning)  //���ͷ
				{
						XT100_battery_temp_state=State_Normal;
						check_temp0_count_warning = 0;
					  check_temp0_count_err = 0;
				}
				else if(temperature0_interface_battery>Temperature_Err)
				{
						check_temp0_count_warning++;
					  check_temp0_count_err++;
					  if(check_temp0_count_err >= 20)
						{
							check_temp0_count_err = 20;
							XT100_battery_temp_state=State_Err;
						}
				}
				else
				{
					 check_temp0_count_warning++;
					 check_temp0_count_err = 0;
					 if(check_temp0_count_warning >= 20) // 10 = 1s
					 {
						 check_temp0_count_warning = 20;
						 XT100_battery_temp_state=State_Warning;
					 }
				}
				
				if(temperature1_interface_battery<Temperature_Warning)   //С��ͷ
				{
						XT90_battery_temp_state=State_Normal;
						check_temp1_count_warning=0;
					  check_temp1_count_err = 0;
				}
				else if(temperature1_interface_battery>Temperature_Err)
				{
						check_temp1_count_warning++;
					  check_temp1_count_err++;
					  if(check_temp1_count_err > 20)
					  {
					    check_temp1_count_err = 20;
							XT90_battery_temp_state=State_Err;
					  }
				}
				else
				{
						check_temp1_count_warning++;
					  check_temp1_count_err = 0;
					  if(check_temp1_count_warning >= 20) // 10 = 1s
						{
							 check_temp1_count_warning = 20;
						   XT90_battery_temp_state = State_Warning;
						}
				}
    }  
}


/***********************************����APP��ң���� �ջ���������Ƶ�� ***********************************************************/

#define FOC1_MIN_VALUE_WORKING Get_Spary_Limit_Value(FOC_1,MIN_SPRAY_ID)
#define FOC1_MAX_VALUE_WORKING Get_Spary_Limit_Value(FOC_1,MAX_SPRAY_ID)
#define FOC2_MIN_VALUE_WORKING Get_Spary_Limit_Value(FOC_2,MIN_SPRAY_ID)
#define FOC2_MAX_VALUE_WORKING Get_Spary_Limit_Value(FOC_2,MAX_SPRAY_ID)

/***********************************
ʹ������ʱ5HZ��ִ��Ƶ��
***********************************/
uint8_t control_freq = 0;
float target_capa_debug = 0;
uint8_t debug_target_pules;
void Set_Foc_Value_By_Flow_Double_Pump(spray_sys_esc_status_t foc_status ) //��λΪ 0.02L/min
{
	 static float value_1 = 0,value_2 = 0;
	 static uint8_t last_flow = 0,last_status = 0;
	 float  target_pules = 0,step_value_pulse = 0,target_flow= 0,wucha = 0;
	 static uint8_t state = 0;

    control_freq++;
		if(foc_status.enable_flag ==1 && foc_status.flow_speed != 0)
		{
			if(foc_status.flow_speed > MAX_FLOW )//2.54 L/min
				foc_status.flow_speed = MAX_FLOW;
			if(foc_status.flow_speed < MIN_FLOW)//0.4 L/min
				foc_status.flow_speed = MIN_FLOW;

			target_flow = foc_status.flow_speed *0.02;
				

			if(last_flow == 0)
			{
				value_1 = FOC1_MIN_VALUE_WORKING;
				value_2 = FOC2_MIN_VALUE_WORKING; 
			}
			
			if(control_freq % 5 == 0 ) // 1hz
			{
				
				target_capa_debug +=  1000*foc_status.flow_speed *0.02f/60;
				// ���Kֵ���ԣ��ڱ仯��Χ����У׼������ֵ������
				 if(last_status == 1)
				 { 
				  target_pules = (float)((1000*foc_status.flow_speed *0.02f)/(Volume_Each_Pulse*60))+0.5f;//�������� 
					wucha = current_capacity_total - target_capa_debug;
					switch(state)
					{
					   case 0:
							 if(wucha > 150)
								 state = 1;
							 else if(wucha < -100)
								 state = 2;
							 else
								 state = 0;
						 break;
					
						 case 1:
						  target_pules = target_pules - 5;
						  if(wucha < -100)
								state = 0;
						 break;
						 
						 case 2:
							target_pules = target_pules + 5;	
						  if(wucha > 50)
								state = 0; 
						 break;
						 
						 default:
							
						 break;
				
					}
					
					debug_target_pules = target_pules;
					if((fabs(target_pules - get_cali_flow_freq())+0.5f) > 1 )//�����������̫����ֹͣУ׼
						step_value_pulse = (float)(target_pules - get_cali_flow_freq());

					
					if(step_value_pulse >= 10)
						step_value_pulse = 10; // �˴���ʾ��Ϊ�ٷֱ�����
					if(step_value_pulse <= -10)
						step_value_pulse = -10; // �˴���ʾ��Ϊ�ٷֱ�����
	
				 }
				 else
				 {
						last_status = 1;// ����ϴ���ͣ �ӻ�һ����������
				 }
				 
				 { 
					 extern uint8_t start_flag;
					 start_flag = 2; // �ر��������ӡ
				 }
				 
				 		// ȷ����Χ��ͬ������
					value_1+= (FOC1_MAX_VALUE_WORKING - FOC1_MIN_VALUE_WORKING)*(step_value_pulse/100);
					value_2+= (FOC2_MAX_VALUE_WORKING - FOC2_MIN_VALUE_WORKING)*(step_value_pulse/100);
			
	        uart_printf( 2, "ve_1 = %.2f ve_2 = %.2f step_p = %.2f idx_7 = %d  tart_p = %.2f tar_f = %.2f current_t = %d tar_c_d = %.2f\r\n",
	        value_1,value_2,step_value_pulse ,flow_sensor_value,target_pules,target_flow,current_capacity_total,target_capa_debug);
				  uart_printf( 2, "#####press1_value_min = %d  press0_value_min = %d  \r\n", press1_value_min,press0_value_min);
				  uart_printf( 2, " \r\n");
			}
		

			last_flow = foc_status.flow_speed;
			
			if(value_1 < FOC1_MIN_VALUE_WORKING)
				value_1 = FOC1_MIN_VALUE_WORKING;
			if(value_2 < FOC2_MIN_VALUE_WORKING)
				value_2 = FOC2_MIN_VALUE_WORKING;

			if(value_1 > FOC1_MAX_VALUE_WORKING)
				value_1 = FOC1_MAX_VALUE_WORKING;
			if(value_2 > FOC2_MAX_VALUE_WORKING)
				value_2 = FOC2_MAX_VALUE_WORKING;
			
			Set_Throttle_Value_1(value_1);
			Set_Throttle_Value_2(value_2);
		}
		else
		{
			Set_Throttle_Value_1(MIN_CNT);
			Set_Throttle_Value_2(MIN_CNT);
			last_status = 0;
			control_freq = 0;
		}
}


void Set_Foc_Value_By_Flow_Single_Pump(uint8_t foc_id,spray_sys_esc_status_t foc_status ) //��λΪ 0.02L/min
{
	 static uint8_t control_freq_1 = 0,control_freq_2 = 0;
	 static float value_1 = 0,value_2 = 0;
	 static uint8_t last_flow_1 = 0,last_status_1 = 0;
	 static uint8_t last_flow_2 = 0,last_status_2 = 0;
   float  target_pules_1 = 0,step_value_pulse_1 = 0;
	 float  target_pules_2 = 0,step_value_pulse_2 = 0,target_flow = 0;
	
   if(foc_id == FOC_1)
	 {
		 control_freq_1++;
		 if(foc_status.enable_flag ==1 && foc_status.flow_speed != 0)
		 {
		 	if(foc_status.flow_speed > MAX_FLOW )//2.54 L/min
				foc_status.flow_speed = MAX_FLOW;
			if(foc_status.flow_speed < MIN_FLOW)//0.4 L/min
				foc_status.flow_speed = MIN_FLOW;
			
			if(Volume_Each_Pulse != 0) // �����ϵ�Ĭ��һ������ֵ
				target_pules_1 = (float)((1000*foc_status.flow_speed *0.02f)/(Volume_Each_Pulse*60))+0.5f;//��������
			
		  if(last_flow_1 == 0)
				value_1 = FOC1_MIN_VALUE_WORKING;
			
			target_flow = foc_status.flow_speed*0.02;
			if(control_freq_1 % 5 == 0 ) // 1hz
			{
				// ���Kֵ���ԣ��ڱ仯��Χ����У׼������ֵ������
				
				 target_capa_debug +=  1000*foc_status.flow_speed *0.02f/60;
				 if(last_status_1 == 1)
				 {
					if((fabs(target_pules_1 - get_cali_flow_freq())+0.5f) > 1 )//�����������̫����ֹͣУ׼
						step_value_pulse_1 = (float)(target_pules_1 - get_cali_flow_freq());
					
					
					if(step_value_pulse_1 >= 10)
						step_value_pulse_1 = 10; // �˴���ʾ��Ϊ�ٷֱ�����
					if(step_value_pulse_1 <= -10)
						step_value_pulse_1 = -10; // �˴���ʾ��Ϊ�ٷֱ�����
				
				 }
				 else
				 {
						last_status_1 = 1;// ����ϴ���ͣ �ӻ�һ����������
				 }
				 { 
					 extern uint8_t start_flag;
					 start_flag = 2; // �ر��������ӡ
				 }
				 
				 				 		// ȷ����Χ��ͬ������
			    value_1+= (FOC1_MAX_VALUE_WORKING - FOC1_MIN_VALUE_WORKING)*(step_value_pulse_1/100);
				  uart_printf( 2, " 1--v_1 = %.2f step_p = %.2f idx_7 = %d  tar_p_1 = %.2f tar_flow = %.2f cur_total = %d tar_capa = %.2f \r\n",
	        value_1,step_value_pulse_1 ,flow_sensor_value,target_pules_1,target_flow,current_capacity_total,target_capa_debug);
				  uart_printf( 2, "#####press1_value_min = %d  press0_value_min = %d  \r\n", press1_value_min,press0_value_min);
				  uart_printf( 2, " \r\n");

			}
			
	
			  
   			last_flow_1 = foc_status.flow_speed;
				 if(value_1 < FOC1_MIN_VALUE_WORKING)
					 value_1 = FOC1_MIN_VALUE_WORKING;
				 if(value_1 > FOC1_MAX_VALUE_WORKING)
					 value_1 = FOC1_MAX_VALUE_WORKING;
				 Set_Throttle_Value_1(value_1);
		 }
		 else
		 {
			 	last_status_1 = 0;
			  control_freq_1 = 0;
			  Set_Throttle_Value_1(MIN_CNT);
		 }
		 Set_Throttle_Value_2(MIN_CNT);
	 }
	 else
	 {
		 control_freq_2++;
		 if(foc_status.enable_flag ==1 && foc_status.flow_speed != 0)
		 {
		 	if(foc_status.flow_speed > MAX_FLOW )//2.54 L/min
				foc_status.flow_speed = MAX_FLOW;
			if(foc_status.flow_speed < MIN_FLOW)//0.4 L/min
				foc_status.flow_speed = MIN_FLOW;
			
			if(Volume_Each_Pulse != 0) // �����ϵ�Ĭ��һ������ֵ
				target_pules_2 = (float)((1000*foc_status.flow_speed *0.02f)/(Volume_Each_Pulse*60))+0.5f;//��������
			
		  if(last_flow_2 == 0)
				value_2 = FOC2_MIN_VALUE_WORKING;
			
			target_flow = foc_status.flow_speed*0.02 ;
			if(control_freq_2 % 5 == 0 ) // 1 hz
			{
				// ���Kֵ���ԣ��ڱ仯��Χ����У׼������ֵ������
				 target_capa_debug +=  1000*foc_status.flow_speed *0.02f/60;
				 if(last_status_2 == 1)
				 {
					if((fabs(target_pules_2 - get_cali_flow_freq())+0.5f) > 1 )//�����������̫����ֹͣУ׼
						step_value_pulse_2 = (float)(target_pules_2 - get_cali_flow_freq());
					
					
					if(step_value_pulse_2 >= 10)
						step_value_pulse_2 = 10; // �˴���ʾ��Ϊ�ٷֱ�����
					if(step_value_pulse_2 <= -10)
						step_value_pulse_2 = -10; // �˴���ʾ��Ϊ�ٷֱ�����
					
				 }
				 else
				 {
						last_status_2 = 1;// ����ϴ���ͣ �ӻ�һ����������
				 }
				 
				 
				 { 
					 extern uint8_t start_flag;
					 start_flag = 2; // �ر��������ӡ
				 }
				  // ȷ����Χ��ͬ������
			    value_2+= (FOC2_MAX_VALUE_WORKING - FOC2_MIN_VALUE_WORKING)*(step_value_pulse_2/100);
				  uart_printf( 2, " 2--v2 = %.2f step_p_2 = %.2f idx_7 = %d tar_p_2 = %.2f tar_f = %.2f curr_c = %d tar_c = %.2f \r\n",
	        value_2,step_value_pulse_2 ,flow_sensor_value,target_pules_2,target_flow,current_capacity_total,target_capa_debug);
				  uart_printf( 2, "#####press1_value_min = %d  press0_value_min = %d  \r\n", press1_value_min,press0_value_min);
				  uart_printf( 2, " \r\n");
			}
			// ȷ����Χ��ͬ������
			 last_flow_2 = foc_status.flow_speed;
			
			 if(value_2 < FOC2_MIN_VALUE_WORKING)
				value_2 = FOC2_MIN_VALUE_WORKING;
			 
			 if(value_2 > FOC2_MAX_VALUE_WORKING)
				value_2 = FOC2_MAX_VALUE_WORKING;
			 
			 Set_Throttle_Value_2(value_2);
		 }
		 else
		 {
			
			 	last_status_2 = 0;
			  control_freq_2 = 0;
			  Set_Throttle_Value_2(MIN_CNT);
		 }
		 Set_Throttle_Value_1(MIN_CNT);
	 }

}



/*********************************** ����У׼ʱ�ջ���������Ƶ�� ***********************************************************/
void Control_The_Frequency_Pulse_1(uint16_t foc_value,uint8_t target_value)
{
	 static float control_value = 0;
	 static uint8_t last_target_value = 0;//control_count = 0
	 float step_value_pulse = 0;
	
	 if(last_target_value != target_value) // ��������Ƶ�ʺ�������������ֵ
		 control_value = foc_value;
	
	 if(abs(target_value - get_cali_flow_freq()) > 2 && (get_cali_flow_freq() > (target_value - 8)))//�����������̫����ֹͣУ׼
		 step_value_pulse = (float)P_FREQ*(target_value - get_cali_flow_freq());
	 
	 control_value += step_value_pulse;
	 
	 Set_Throttle_Value_1(control_value);
	 last_target_value = target_value;
	 uart_printf( 2, "control_value_1 = %.2f  step_value_pulse_1 = %.2f idx_7_freq = %d  target_value_1 = %d  press0_value_min = %d  \r\n",
	 control_value,step_value_pulse ,flow_sensor_value,target_value,press0_value_min);
}

void Control_The_Frequency_Pulse_2(uint16_t foc_value,uint8_t target_value)
{
	 static float control_value = 0;
	 static uint8_t last_target_value = 0;//control_count = 0
	 float step_value_pulse = 0;
	
	 if(last_target_value != target_value) // ��������Ƶ�ʺ�������������ֵ
		 control_value = foc_value;
	 

	 if(abs(target_value - get_cali_flow_freq()) > 2 && (get_cali_flow_freq() > (target_value - 8)))//�����������̫����ֹͣУ׼
		 step_value_pulse = (float)P_FREQ*(target_value - get_cali_flow_freq());
	 
	 control_value += step_value_pulse;
	 
	 Set_Throttle_Value_2(control_value);
	 last_target_value = target_value;
	 uart_printf( 2, "control_value_2 = %.2f  step_value_pulse_2 = %.2f idx_7_freq = %d  target_value_2 = %d  press1_value_min_2 = %d  \r\n",
	 control_value,step_value_pulse ,flow_sensor_value,target_value,press1_value_min);
}

/*************************************����ϵͳ�����Լ�***********************************************************/
// ѹ�������Լ�ģ��
#define DEFAULT_FOC_VALUE 1210
void spray_system_press_para_init_check(void)
{
	 cali_pressure1_para_read_flash((uint8_t*)&spray_system_pressure_cali_para_1,sizeof(spray_system_pressure_cali_para_t));
	 cali_pressure2_para_read_flash((uint8_t*)&spray_system_pressure_cali_para_2,sizeof(spray_system_pressure_cali_para_t));
	 if(  spray_system_pressure_cali_para_1.save_flag == Data_Exists_Flag
			  &&spray_system_pressure_cali_para_1.throttle_value_min <=  spray_system_pressure_cali_para_1.throttle_value_default
		    && spray_system_pressure_cali_para_1.throttle_value_default <=  spray_system_pressure_cali_para_1.throttle_value_max
		    && spray_system_pressure_cali_para_1.throttle_value_min > DEFAULT_FOC_VALUE - 50
		    && spray_system_pressure_cali_para_1.throttle_value_max < MAX_CNT - 200)
		{
		  uart_printf( 2 ,"spray_system_press1_para_init_check_pass \r\n");                     
		}
		else
		{	
			spray_system_pressure_cali_para_1.save_flag = Data_Exists_Flag;//�����쳣 �ָ�Ĭ�ϲ��� ����У׼
			spray_system_pressure_cali_para_1.need_cali_flag = 1;
		  spray_system_pressure_cali_para_1.throttle_value_min = DEFAULT_FOC_VALUE;
			spray_system_pressure_cali_para_1.throttle_value_default = DEFAULT_FOC_VALUE + 30;
			spray_system_pressure_cali_para_1.throttle_value_max = DEFAULT_FOC_VALUE + 60;
			spray_system_pressure_cali_para_1.pressure_user = 0xaa;
			cali_pressure1_para_write_flash((uint8_t*)&spray_system_pressure_cali_para_1,sizeof(spray_system_pressure_cali_para_t));
			uart_printf( 2 ,"spray_system_press1_para_init_check_fail \r\n");
		}		
		
		
		if( spray_system_pressure_cali_para_2.save_flag == Data_Exists_Flag
			  &&spray_system_pressure_cali_para_2.throttle_value_min <=  spray_system_pressure_cali_para_2.throttle_value_default
		    && spray_system_pressure_cali_para_2.throttle_value_default <=  spray_system_pressure_cali_para_2.throttle_value_max
		    && spray_system_pressure_cali_para_2.throttle_value_min > DEFAULT_FOC_VALUE - 50
		    && spray_system_pressure_cali_para_2.throttle_value_max < MAX_CNT - 200)
		{
		  uart_printf( 2 ,"spray_system_press2_para_init_check_pass \r\n");                     
		}
		else
		{	
			spray_system_pressure_cali_para_2.save_flag = Data_Exists_Flag;//�����쳣 �ָ�Ĭ�ϲ��� ����У׼
			spray_system_pressure_cali_para_2.need_cali_flag = 1;
		  spray_system_pressure_cali_para_2.throttle_value_min = DEFAULT_FOC_VALUE;
			spray_system_pressure_cali_para_2.throttle_value_default = DEFAULT_FOC_VALUE + 30;
			spray_system_pressure_cali_para_2.throttle_value_max = DEFAULT_FOC_VALUE + 60;
			spray_system_pressure_cali_para_2.pressure_user = 0xaa;
			cali_pressure2_para_write_flash((uint8_t*)&spray_system_pressure_cali_para_2,sizeof(spray_system_pressure_cali_para_t));
			uart_printf( 2 ,"spray_system_press2_para_init_check_fail \r\n");
		}	
		Set_Spary_Limit_Value(FOC_1,MAX_SPRAY_ID,spray_system_pressure_cali_para_1.throttle_value_max);	
		Set_Spary_Limit_Value(FOC_1,MIN_SPRAY_ID,spray_system_pressure_cali_para_1.throttle_value_min);	
		
		Set_Spary_Limit_Value(FOC_2,MAX_SPRAY_ID,spray_system_pressure_cali_para_2.throttle_value_max);	
		Set_Spary_Limit_Value(FOC_2,MIN_SPRAY_ID,spray_system_pressure_cali_para_2.throttle_value_min);	
}



void set_cali_capaciy(uint16_t value)
{
  spray_system_flow_cali_para1.capacity = value - 600;// 600ml Ĭ�ϴ���Һλ����
}

void spray_sysem_flow_para_init_check(void)
{
  cali_flow_para1_read_flash((uint8_t*)&spray_system_flow_cali_para1,sizeof(spray_system_flow_cali_para_t));
  if(spray_system_flow_cali_para1.save_flag == Data_Exists_Flag
	  &&spray_system_flow_cali_para1.capacity <= 10000
	  &&spray_system_flow_cali_para1.capacity >= 3000
	  &&spray_system_flow_cali_para1.cali_pulses < 100000
    &&spray_system_flow_cali_para1.cali_pulses > 100  
    &&spray_system_flow_cali_para1.cali_time < 1000*60*20 // 20 min
    &&spray_system_flow_cali_para1.cali_time > 1000*60 )  //  1 min 
  {
     uart_printf( 2 ,"*************flow1_spray_system_cali_para_check_pass \r\n");
		 Cali_Freq =  spray_system_flow_cali_para1.cali_freq;//(float)spray_system_flow_cali_para1.cali_pulses*1000/(float)spray_system_flow_cali_para1.cali_time;
	}
	else
	{
	   spray_system_flow_cali_para1.save_flag = Data_Exists_Flag;
		 spray_system_flow_cali_para1.cali_pulses = 14692;
		 spray_system_flow_cali_para1.capacity = 7350;
		 Cali_Freq = 60; // Ĭ����ˮ�Ĳ���
	   uart_printf( 2 ,"*************flow1_spray_system_cali_para_check_fail \r\n");
	}
   user_volume =  (float)spray_system_flow_cali_para1.capacity/(float)(spray_system_flow_cali_para1.cali_pulses);
	 Set_Volume_Each_Pulse(user_volume);
}


void spray_residual_volume_init_check(void)
{
	 uint8_t buf[1] = {0}; // 1 
	 cali_volume_para_read_flash(buf, 1); 
	 
	 if(buf[0] == 0xff)
     set_residual_volume_warning_mode(Small_Liquid); 
	 else
		 set_residual_volume_warning_mode(buf[0]);
}

/****************************** �����Ʊ궨����***************************************/
//��ѹ���ȶ���0.3 MP
// APP ��������У׼����
extern uint16_t Throttle_Value_1,Throttle_Value_2;
uint8_t flow_cali_enbale_flag = 0;
float user_pulses2,user_flow2;
float user_pulses1,user_flow1;
uint8_t flow1_cali_compled_flag,flow2_cali_compled_flag;
void set_flow_cali_flag(uint8_t state)
{
	flow_cali_enbale_flag = state;
}
uint8_t debug_flow1_ss;
uint8_t debug_flow2_ss;
uint8_t  Pressure1_check_completed;
uint8_t  Pressure2_check_completed; // ѹ����� ע�����ֱ��� ����һ��Ҫ��
extern uint16_t debug_throttle_value_1,debug_throttle_value_2;
extern cmd_foc_ack_t  debug_stFOC_Value_1,debug_stFOC_Value_3;
void flow1_calibration_process(void)
{
	 static uint8_t flow_cali_step = 0;
	 static uint8_t freq_control_count = 0;
	 static uint16_t switch_count = 0;
	 static uint16_t flow_cali_value = MIN_CNT;
	 freq_control_count++;
	// APP ��������У׼����
	debug_flow1_ss  = flow_cali_step;
   switch(flow_cali_step)
	 {
	   case 0:
		      // if(�ɿ���ɣ�������־ʹ�� �������ѹ��У׼)	 
		   if( flow_cali_enbale_flag == 1
				   && FOC_1_STATUS.valid_spray == 1 
				   && FOC_1_STATUS.enable_flag == START_SPARY)
			 {
				 flow_cali_enble();
				 //SET_FOC1_MUTEX_FLAG(0); // ��ȡˮ�ÿ���Ȩ
				 flow_cali_step = 1;
				 press0_cali_para_reset();
				 //WORKING_PRESSURE_DEFAULT
			 }	 
		 break;
			 
		 case 1:
			 
			 if(FOC_1_STATUS.enable_flag == START_SPARY && flow_cali_value == MIN_CNT)
			 {
				   SET_FOC1_MUTEX_FLAG(0); // ��ȡˮ�ÿ���Ȩ
				   //SET_FOC2_MUTEX_FLAG(0); // ��ȡˮ�ÿ���Ȩ
					 flow_cali_value = Set_FOC1_Value_By_Pressure0(spray_system_pressure_cali_para_1.throttle_value_default,WORKING_PRESSURE_DEFAULT);
			 }
			 else if(FOC_1_STATUS.enable_flag == STOP_SPARY)
			 {
					SET_FOC1_MUTEX_FLAG(1); // �ͷ�ˮ�ÿ���Ȩ
				  //SET_FOC2_MUTEX_FLAG(1);
			 }
			 
		   if(flow_cali_value == MIN_CNT)
			 {
			    //����ѹ��У׼
			 }
			 else if(flow_cali_value > MIN_CNT)
			 {
				 switch_count++;
				 Pressure1_check_completed = 1;
          // ȡ���ʱ���������ΪĿ��Ƶ��һ��ʱ��
				 if(switch_count >= 80) //�ȶ� ��֤���������ȶ�
				 {
					 switch_count = 80;
					 if(Pressure2_check_completed == 1)
					 {
						 switch_count = 0;
				     flow_cali_step = 2;
					 }
				 }		
			 }
		   else // ѹ��У׼ʧ��
			 {
			   // ��APP �ƶ�У׼ʧ��,����ѭ����ȡ��APP ����
			 }
			 			 
			 if(flow_cali_enbale_flag == 0) // ȡ��У׼
			 {
			    SET_FOC1_MUTEX_FLAG(1); //�ͷ�ˮ�ÿ���Ȩ
				  flow_cali_clear();
				  flow_cali_step = 5;
			 }
     break;			 
			 
		 
		 case 2:	
		   // if (ѹ���������߹�С���� 5s �У� ���ٴ�����У׼)
		 	 if(flow_cali_enbale_flag == 0) // ȡ��У׼
			 {
			    SET_FOC1_MUTEX_FLAG(1); //�ͷ�ˮ�ÿ���Ȩ
				  flow_cali_clear();
				  flow_cali_step = 5;
			 }
			 {
			   extern uint8_t start_flag;
			   start_flag = 1;
			 }
		 
		   // ���ʹ������ִ�����
				if(FOC_1_STATUS.enable_flag == START_SPARY && (FOC_1_STATUS.flow_speed != 0))
				{					 
					 if(freq_control_count % 20 == 0)
				   {
					
						  Control_The_Frequency_Pulse_1(flow_cali_value,Med_Frequency_Pulse);
         			uart_printf( 2, "cali_pulses_1 = %d ***** cali_time_1 = %d  current_capacity_total = %d \r\n",
						  spray_system_flow_cali_para1.cali_pulses,spray_system_flow_cali_para1.cali_time,current_capacity_total);		
              uart_printf( 2, "acc_signal  = %d  debug_throttle_value_2 = %d \r\n", debug_stFOC_Value_3.acc_signal , debug_throttle_value_2);	
	            uart_printf( 2, " \r\n");
					 }
				   SET_FOC1_MUTEX_FLAG(0); // ��ȡˮ�ÿ���Ȩ
				}
				else
				{
					 freq_control_count = 1; // �����������������������
				   SET_FOC1_MUTEX_FLAG(1); // �ͷ�ˮ�ÿ���Ȩ
				}
				
			 spray_system_flow_cali_para1.cali_pulses = get_cali_flow_pulses_total();
			 spray_system_flow_cali_para1.cali_time = get_cail_flow_time();
			 spray_system_flow_cali_para1.cali_freq = Med_Frequency_Pulse;
				
			
		   if(capacity_flag_1L == 1 )
			 {
				 //spray_system_flow_cali_para.capacity  = get_cali_capacity();
				 user_pulses1 = 1000* ((float)spray_system_flow_cali_para1.cali_pulses /(float)spray_system_flow_cali_para1.cali_time);
		     user_flow1 = 60*(float)((float)spray_system_flow_cali_para1.capacity)/((float)spray_system_flow_cali_para1.cali_time);
				 SET_FOC1_MUTEX_FLAG(1); // �ͷ�ˮ�ÿ���
				 flow1_cali_compled_flag = 1;
				 flow_cali_step = 3; 
			 }
		 break;
		 
		 case 3:
		   cali_flow_para1_write_flash((uint8_t*)&spray_system_flow_cali_para1,sizeof(spray_system_flow_cali_para_t));
		   uart_printf( 2, "************************flow1_para_write_flash****************\r\n" );
		   flow_cali_step = 4;	
		 	 if(flow2_cali_compled_flag  && flow1_cali_compled_flag)
			 {
		      user_volume =  (float)spray_system_flow_cali_para1.capacity/(float)(spray_system_flow_cali_para1.cali_pulses);
	        Set_Volume_Each_Pulse(user_volume);
			    set_flow_cali_result(flow_cali_complted);
			 }
		 break;
		 
		 case 4:
		   if(freq_control_count % 10 == 0)
			 {
				 uart_printf( 2, "cali_pulses_1 =  %d   cali_time_1 = %d  user_volume = %.3f  current_capacity_total = %d \r\n",
										 spray_system_flow_cali_para1.cali_pulses,spray_system_flow_cali_para1.cali_time,user_volume,current_capacity_total);
				 //uart_printf( 2, "user_pulses_5L =  %.2f   user_flow_5L = %.2f  \r\n",user_pulses2,user_flow2 );
			 }
      // ������У׼��,�ȴ���������
		 break;
			 
		  case 5:  
		  if( flow_cali_enbale_flag == 1) // �ٴο�ʼУ׼
			{
				  flow_cali_step = 0;
				  SET_FOC1_MUTEX_FLAG(1); // �ͷ�ˮ�ÿ���
					flow1_cali_compled_flag = 0;
			    flow2_cali_compled_flag = 0;
			}
		  break;
		 
		 default:
		  
		 break;
	 }
}
/*********************************************************************************************************
*	�� �� ��:uint16_t MedFilter(*Value_buf)
*	����˵��:��ֵ�˲�
*	��    �Σ���
*	�� �� ֵ: uint16_t
*********************************************************************************************************/
#define 	M 81
uint8_t flow_temp[81];
uint8_t Flow_Freq_MedFilter(uint8_t *Value_buf)
{
	uint16_t temp=0;
	uint8_t i,j; 
	for (j=0;j<M-1;j++) 
	{ 
		for (i=0;i<M-j;i++) 
	  {    
			if ( Value_buf[i]>Value_buf[i+1] )
		  {        
				temp = Value_buf[i]; 
				Value_buf[i] = Value_buf[i+1]; 
				Value_buf[i+1] = temp;   
		  } 
	  } 
	}  
	return Value_buf[(M-1)/2];	
}


void flow2_calibration_process(void)
{
	 static uint8_t flow_cali_step = 0;
	 static uint16_t freq_control_count = 0,switch_count = 0;
	 static uint16_t flow_cali_value = MIN_CNT;
	 freq_control_count++;
	// APP ��������У׼����
	 debug_flow2_ss  = flow_cali_step;
   switch(flow_cali_step)
	 {
	   case 0:
		      // if(�ɿ���ɣ�������־ʹ�� �������ѹ��У׼)	 
		   if( flow_cali_enbale_flag == 1
				   && FOC_2_STATUS.valid_spray == 1 
				   && FOC_2_STATUS.enable_flag == START_SPARY)
			 {
				 flow_cali_enble();
				 //SET_FOC2_MUTEX_FLAG(0); // ��ȡˮ�ÿ���Ȩ
				 flow_cali_step = 1;
				 press1_cali_para_reset();
			 }	 
		 break;
			 
		 case 1:
		 	 if(FOC_2_STATUS.enable_flag == START_SPARY && flow_cali_value == MIN_CNT)
			 {
					if(Pressure1_check_completed == 1) // ������һ����������ѹ�����ٿ�ʼִ��ѹ������
					{
					  SET_FOC2_MUTEX_FLAG(0); // ��ȡˮ�ÿ���Ȩ
						flow_cali_value = Set_FOC2_Value_By_Pressure1(spray_system_pressure_cali_para_2.throttle_value_default,WORKING_PRESSURE_DEFAULT);
					}
					else
					{
					  SET_FOC2_MUTEX_FLAG(1); // �ͷ�ˮ�ÿ���Ȩ
					}		
		   }
			 else if(FOC_2_STATUS.enable_flag == STOP_SPARY)
			 {
			    SET_FOC2_MUTEX_FLAG(1); // �ͷ�ˮ�ÿ���Ȩ
			 }
			 
		   if(flow_cali_value == MIN_CNT)
			 {
			    //����ѹ��У׼
			 }
			 else if(flow_cali_value > MIN_CNT)
			 {
				 if(switch_count <  M)
				 {
					 flow_temp[switch_count] = get_cali_flow_freq();
				   switch_count++;
					 {
						 extern uint8_t start_flag;
						 start_flag = 0;
				   }
				 }
				 else          // ȡ���ʱ���������ΪĿ��Ƶ��  //�ȶ����� ��֤���������ȶ�
				 {
					 Med_Frequency_Pulse =  Flow_Freq_MedFilter(flow_temp);
					 Pressure2_check_completed = 1;
					 switch_count = 0;
				   flow_cali_step = 2;
					 {
						 extern uint8_t start_flag;
						 start_flag = 1;
				   }
				 }		
			 }
		   else // ѹ��У׼ʧ��
			 {
			   // ��APP �ƶ�У׼ʧ��,����ѭ����ȡ��APP ����
			 }
			 			 
			 if(flow_cali_enbale_flag == 0) // ȡ��У׼
			 {
			    SET_FOC2_MUTEX_FLAG(1); //�ͷ�ˮ�ÿ���Ȩ
				  flow_cali_clear();
				  flow_cali_step = 5;
			 }
     break;			 
			 
		 
		 case 2:	
		   // if (ѹ���������߹�С���� 5s �У� ���ٴ�����У׼)
		 	 if(flow_cali_enbale_flag == 0) // ȡ��У׼
			 {
			    SET_FOC2_MUTEX_FLAG(1); //�ͷ�ˮ�ÿ���Ȩ
				  flow_cali_clear();
				  flow_cali_step = 5;
			 }
			 
			  {
			   extern uint8_t start_flag;
			   start_flag = 1;
			  }
		 
		   // ���ʹ������ִ����
				if(FOC_2_STATUS.enable_flag == START_SPARY && (FOC_2_STATUS.flow_speed != 0))
				{				
			    if(freq_control_count % 20 == 0)
				  {
						  Control_The_Frequency_Pulse_2(flow_cali_value,Med_Frequency_Pulse);// ����Ϊ������1��Ŀ������
         			uart_printf( 2, "cali_pulses_2 = %d ***** cali_time_2 = %d  current_capacity_total = %d  \r\n",
						  spray_system_flow_cali_para1.cali_pulses,spray_system_flow_cali_para1.cali_time,current_capacity_total);
						  uart_printf( 2, "acc_signal  = %d  debug_throttle_value_2 = %d \r\n",  debug_stFOC_Value_3.acc_signal , debug_throttle_value_2);		
	            uart_printf( 2, " \r\n");
					}					
				  SET_FOC2_MUTEX_FLAG(0); // ��ȡˮ�ÿ���Ȩ
				}
				else
				{
					 freq_control_count = 1; // �����������������������
				   SET_FOC2_MUTEX_FLAG(1); // �ͷ�ˮ�ÿ���Ȩ
				}

		   if(capacity_flag_1L == 1 )
			 {
				 SET_FOC2_MUTEX_FLAG(1); // �ͷ�ˮ�ÿ���Ȩ
				 flow2_cali_compled_flag = 1;
				 flow_cali_step = 3; 
			 }
		 break;
		 
		 case 3:
		   uart_printf( 2, "************************flow_para_write_flash****************\r\n" );
		   flow_cali_step = 4;	
		   if(flow2_cali_compled_flag  && flow1_cali_compled_flag)
			 {
          user_volume =  (float)spray_system_flow_cali_para1.capacity/(float)(spray_system_flow_cali_para1.cali_pulses);
	        Set_Volume_Each_Pulse(user_volume);
			    set_flow_cali_result(flow_cali_complted);
			 }
		  
		 break;
		 
		 case 4:
		   if(freq_control_count % 10 == 0)
			 {
				 uart_printf( 2, "cali_pulses_2 =  %d   cali_time_2 = %d  user_volume = %.3f current_capacity_total = %d\r\n",
										 spray_system_flow_cali_para1.cali_pulses,spray_system_flow_cali_para1.cali_time,user_volume,current_capacity_total);
			 }
      // ������У׼��,�ȴ���������
		 break;
			 
		  case 5:  
		  if( flow_cali_enbale_flag == 1) // �ٴο�ʼУ׼
			   {
           flow_cali_step = 0;
					 SET_FOC2_MUTEX_FLAG(1); // �ͷ�ˮ�ÿ���Ȩ
					 flow1_cali_compled_flag = 0;
			     flow2_cali_compled_flag = 0;
			   }
		  break;
		 
		 default:
		  
		 break;
	 }
}


/************************************ѹ�����������ݴ���*************************************************************/

uint16_t get_buf_min_value(uint16_t buf[],uint16_t data_num)
{
  uint16_t i = 0,min_value = 0;
	min_value = buf[0];
	for(i = 0; i < data_num;i++)
	{	
	  if(buf[i] < min_value && buf[i] > 350)// 350�Ǿ�ֹʱADC�Ĳ���
		{
		  min_value = buf[i];
		}
	}
  return min_value;
}

uint16_t get_press_value(uint8_t id)
{
  if(id == Press_Sensor_0)
   return press0_value_min;
	if(id == Press_Sensor_1)
	 return press1_value_min;
	return 0;
}


void press_sensor_handle(void) // 500hz��ȡƵ��
{
	  static uint16_t channel0_buf_count = 0,channel1_buf_count = 0;
	 	press0_value = press0_sensor_get();
	  press1_value = press1_sensor_get();

		Voltage_Press_1 = (press1_value*1.2085f );// 2/3 ��ѹ��ԭʼ��ѹ * ��3/2�� (scope_debug/4096)*3300*1.5f
	  Voltage_Press_0 = press0_value*1.2085f;
	
	  press0_value = Voltage_Press_0;
	  press1_value = Voltage_Press_1;
	
		if(press0_value > 4000 || press0_value < 300 ) //  ����������������ô��ĵ�ѹ
			press0_value = last_press0_value;
		if(press1_value > 4000 || press1_value < 300) //  ����������������ô��ĵ�ѹ
			press1_value = last_press1_value;
		
  	ADC_channel0_buf[channel0_buf_count++] = press0_value;
		ADC_channel1_buf[channel1_buf_count++] = press1_value;
 		last_press0_value  = last_press0_value;
		last_press1_value  = last_press1_value;
		if(channel0_buf_count == 49)//  50������ȡ��Сֵ  //ADC_channel0_buf[channel0_buf_count++] 
		{
			channel0_buf_count = 0;
			channel1_buf_count = 0;
			press0_value_min = get_buf_min_value(ADC_channel1_buf,50);
			press1_value_min = get_buf_min_value(ADC_channel0_buf,50);
			memset(ADC_channel0_buf,0,50); // ��������
			memset(ADC_channel1_buf,0,50); // ��������
		}
}

extern void uart_printf(uint8_t uart_n,char *pstr, ...);  // for test
#define NORMAL_WROKING_ADC 840
#define DEAD_ZONE 30
#define K_PRESSURE 0.03

/******************************************����Ŀ�깤��ѹ���õ���Ӧ������  ȷ����ҩ�������****************************************/
uint16_t set_count_1 = 0;
uint8_t  run_foc_count_1 = 0;
static float foc_value_1 = 0;

void press0_cali_para_reset(void)
{
	  set_count_1 = 0;
		run_foc_count_1 = 0;
    foc_value_1 = 1250;
}

uint16_t Set_FOC1_Value_By_Pressure0(uint16_t start_foc_value ,uint16_t target_pressure_value) 
{
	static uint16_t target_pressure_value_last = 0,Last_Press_Sensor_0 = 0;
	static float step_value_1 = 0,step_value_last_1 = 0; 
	static uint16_t calibration_value = 0;

	if(target_pressure_value != target_pressure_value_last)// for debug
	{
	  set_count_1 = 0;
		run_foc_count_1 = 0;
		foc_value_1 = start_foc_value; //ÿ�θı�Ŀ��ѹ��ʱ���������뿪ʼ������ֵ
	}
	
	if(set_count_1 < 7) // 1s
	{
		if(get_press_value(Press_Sensor_0) >= target_pressure_value - DEAD_ZONE 
       && get_press_value(Press_Sensor_0) <= target_pressure_value + DEAD_ZONE )
		{
			set_count_1++;			// �ڴ��ȶ�1���ӣ���õ�һ��ֵ���Դ�Ϊ׼                     
		}
		else
		{
		  run_foc_count_1++;
			set_count_1 = 0;
			step_value_1 = (target_pressure_value - get_press_value(Press_Sensor_0))*K_PRESSURE;
		  if( (Last_Press_Sensor_0 > get_press_value(Press_Sensor_0)) 
		       &&(get_press_value(Press_Sensor_0)<= NORMAL_WROKING_ADC)) // ѹ��˲���С���쳣����
				 step_value_1  = step_value_last_1;
			
			if(step_value_1 < 1 && step_value_1 > 0)
				step_value_1 = 1; 			
			if(step_value_1 < 0 && step_value_1 > -1)
				step_value_1 = -1; 		

			   if(step_value_1 > 15)// for debug 
					 step_value_1 = 15;
				 
			foc_value_1 += step_value_1;   // ����Ƶ����Ҫ���� ��ѹ����Ӧ�Ƚ���
		}
		
		
		if(foc_value_1 >= User_Max_Throttle) // �������ֵ�����򲻵�Ŀ��ֵ
		{
			foc_value_1 = User_Max_Throttle - 400;
		}
		
		Set_Throttle_Value_1(foc_value_1); // ����Ҫʵʱ���� 
		step_value_last_1 =  step_value_1;
		target_pressure_value_last = target_pressure_value;
		Last_Press_Sensor_0 = get_press_value(Press_Sensor_1);
	}
	else
	{
		calibration_value = get_foc_throttle_value(FOC_1);	
		return calibration_value; //������ֵ
	}
	uart_printf( 2 ,"target_pressure_value =%d \r\n", target_pressure_value);
	uart_printf( 2 ,"step_value = %.2f   run_foc_count = %d  set_count = %d   press0_value_min =  %d ,foc_value = %.2f \r\n",
	                 step_value_1,run_foc_count_1,set_count_1,press0_value_min,foc_value_1);
	uart_printf( 2, " \r\n");
	return MIN_CNT; // ������Сֵ����ʾ���ڱ궨
}


uint16_t set_count_2= 0;
uint8_t  run_foc_count_2 = 0;
static float foc_value_2 = 0;
void press1_cali_para_reset(void)
{
	  set_count_2 = 0;
		run_foc_count_2 = 0;
    foc_value_2 = 1250;
}
uint16_t Set_FOC2_Value_By_Pressure1(uint16_t start_foc_value ,uint16_t target_pressure_value) // 10hz
{
	static uint16_t target_pressure_value_last = 0,Last_Press_Sensor = 0;
	static float step_value_2 = 0,step_value_last_2 = 0; 
	static uint16_t calibration_value = 0;

	if(target_pressure_value != target_pressure_value_last)// for debug
	{
	  set_count_2 = 0;
		run_foc_count_2 = 0;
		foc_value_2 = start_foc_value; //ÿ�θı�Ŀ��ѹ��ʱ���������뿪ʼ������ֵ
	}
	
	if(set_count_2 < 7) // 1s
	{
		if(get_press_value(Press_Sensor_1) >= target_pressure_value - DEAD_ZONE 
       && get_press_value(Press_Sensor_1) <= target_pressure_value + DEAD_ZONE )
		{
			set_count_2++;			// �ڴ��ȶ�1���ӣ���õ�һ��ֵ���Դ�Ϊ׼                     
		}
		else
		{
		  run_foc_count_2++;// for debug
			set_count_2 = 0;
			
			step_value_2 = (target_pressure_value - get_press_value(Press_Sensor_1))*K_PRESSURE;
		  if( (Last_Press_Sensor > get_press_value(Press_Sensor_1)) 
		       &&(get_press_value(Press_Sensor_1)<= NORMAL_WROKING_ADC)) // ѹ��˲���С���쳣����
				 step_value_2  = step_value_last_2;
			
			if(step_value_2 < 1 && step_value_2 > 0)
				step_value_2 = 1; 			
			if(step_value_2 < 0 && step_value_2 > -1)
				step_value_2 = -1; 		
			
		   if(step_value_2 > 15)// for debug 
				 step_value_2 = 15;			
			foc_value_2 += step_value_2;   // ����Ƶ����Ҫ���� ��ѹ����Ӧ�Ƚ���
		}
		
		
		if(foc_value_2 >= User_Max_Throttle) // �������ֵ�����򲻵�Ŀ��ֵ
		{
			foc_value_2 = User_Max_Throttle - 400;
		}
		
		Set_Throttle_Value_2(foc_value_2); // ����Ҫʵʱ���� 
		step_value_last_2 =  step_value_2;
		target_pressure_value_last = target_pressure_value;
		Last_Press_Sensor = get_press_value(Press_Sensor_1);
	}
	else
	{
		calibration_value = get_foc_throttle_value(FOC_2);	
		return calibration_value; //������ֵ
	}
	
	uart_printf( 2 ,"target_pressure_value =%d \r\n", target_pressure_value);
	uart_printf( 2 ,"$$$$$$  step_value_2 = %.2f   run_foc_count_2 = %d  set_count = %d   press1_value_min =  %d ,foc_value_2 = %.2f \r\n",
	                 step_value_2,run_foc_count_2,set_count_2,press1_value_min,foc_value_2);
	uart_printf( 2, " \r\n");
	return MIN_CNT; // ������Сֵ����ʾ���ڱ궨
}




#define pressure_check_min          0 
#define pressure_check_default      1 
#define pressure_check_max          2
#define pressure_check_sucessfull   3
#define pressure_check_fail         4 
#define pressure_check_unknow       5  

extern uint16_t Throttle_Value_1;
extern uint16_t Throttle_Value_2; // for print

uint16_t FOC1_MAX_SPRAY_Speed,FOC1_MIN_SPRAY_Speed;
uint16_t FOC2_MAX_SPRAY_Speed,FOC2_MIN_SPRAY_Speed;
void Set_Spary_Limit_Value(uint8_t foc_id,uint8_t value_id ,uint16_t value)
{
	if(foc_id == FOC_1)
		if(value_id == MAX_SPRAY_ID)
			FOC1_MAX_SPRAY_Speed = value;
		else
			FOC1_MIN_SPRAY_Speed = value;
	else
		if(value_id == MAX_SPRAY_ID)
			FOC2_MAX_SPRAY_Speed = value;
		else
			FOC2_MIN_SPRAY_Speed = value;
}

uint16_t Get_Spary_Limit_Value(uint8_t foc_id,uint8_t value_id)
{
	if(foc_id == FOC_1)	
		if(value_id == MAX_SPRAY_ID)
			return FOC1_MAX_SPRAY_Speed + 10;
		else
		 return  FOC1_MIN_SPRAY_Speed- 20;
	else
		if(value_id == MAX_SPRAY_ID)
			return FOC2_MAX_SPRAY_Speed + 10;
		else
		 return  FOC2_MIN_SPRAY_Speed- 20;
			
}

uint8_t pressure1_check_complted = 0;

uint8_t pressure2_check_process(void) // 10hz ����
{	
		#ifdef  DEBUG_FLOW_LEVEL
		static	 uint8_t check_state = pressure_check_unknow; 
	  #else
		static	 uint8_t check_state = pressure_check_min;
	  #endif

	  static uint16_t check_count = 0,timeout_count = 0,test_value = 0;
	  test_value = get_press_value(Press_Sensor_1);
    if(spray_system_pressure_cali_para_2.need_cali_flag == 1)
		{
		    return pressure_check_fail;//��APPӦ����Ҫ����У׼ 
		}
		else
		{
				if(check_state != pressure_check_sucessfull && check_state != pressure_check_fail)
				{
					 #ifdef  DEBUG_FLOW_LEVEL	
					 if(FOC_2_STATUS.enable_flag == STOP_SPARY) // ����sensor id �ж�
					 {			 
						 SET_FOC2_MUTEX_FLAG(1); // �ͷ�ˮ�ÿ���Ȩ
						 check_state = pressure_check_unknow;
					 }
					 if(check_state == pressure_check_unknow 
							&& FOC_2_STATUS.enable_flag == START_SPARY
					    && FOC_2_STATUS.valid_spray == 1
							)//
					 {
						 check_state = pressure_check_min;
						 SET_FOC2_MUTEX_FLAG(0); // ��ȡˮ�ÿ���Ȩ
					 }
					 #endif
				}
				
			switch(check_state)
			{
			  case pressure_check_min:

				  Set_Throttle_Value_2(spray_system_pressure_cali_para_2.throttle_value_min);
				  if(get_press_value(Press_Sensor_1) >= WORKING_PRESSURE_MIN - 2*DEAD_ZONE
							&& get_press_value(Press_Sensor_1 ) <= WORKING_PRESSURE_MIN + 2*DEAD_ZONE )
						check_count++;
					else
					  timeout_count++;
					
					if(check_count > 5) // 1s
					{
					  check_count = 0;
						check_state = pressure_check_default;
						timeout_count = 0;
					}
			
					if(timeout_count > 30) //
					{
						check_count = 0;
						timeout_count = 0;
						check_state = pressure_check_fail;
						SET_FOC2_MUTEX_FLAG(1); // �ͷ�ˮ�ÿ���Ȩ 
					}
					
					
					
			  uart_printf( 2 ,"check_state =%d  check_count = %d  timeout_count = %d perssure = %d  Throttle_Value_2 = %d \r\n", 
			            check_state,check_count,timeout_count,test_value,Throttle_Value_2);
				break;
				
				case pressure_check_default:
	
					Set_Throttle_Value_2(spray_system_pressure_cali_para_2.throttle_value_default); 
				 
				  if(get_press_value(Press_Sensor_1) >= WORKING_PRESSURE_DEFAULT - 2*DEAD_ZONE 
							&& get_press_value(Press_Sensor_1) <= WORKING_PRESSURE_DEFAULT + 2*DEAD_ZONE )
						check_count++;
					else
					  timeout_count++;
					
					if(check_count > 5) // 1s
					{
					  check_count = 0;
						check_state = pressure_check_max;
						timeout_count = 0;
			
					}
			
					if(timeout_count > 30) // 
					{
						check_count = 0;
						timeout_count = 0;
						check_state = pressure_check_fail;
						SET_FOC2_MUTEX_FLAG(1); // �ͷ�ˮ�ÿ���Ȩ
					}
				
			uart_printf( 2 ,"check_state =%d  check_count = %d  timeout_count = %d perssure = %d  Throttle_Value_2 = %d \r\n", 
			            check_state,check_count,timeout_count,test_value,Throttle_Value_2);
				break;
				
				case pressure_check_max:
						Set_Throttle_Value_2(spray_system_pressure_cali_para_2.throttle_value_max); 
				  if(get_press_value(Press_Sensor_1) >= WORKING_PRESSURE_MAX - 2*DEAD_ZONE 
							&& get_press_value(Press_Sensor_1) <= WORKING_PRESSURE_MAX + 2*DEAD_ZONE )
						check_count++;
					else
					  timeout_count++;
					
					if(check_count > 5) // 1s
					{
					  check_count = 0;
						check_state = pressure_check_sucessfull;
						SET_FOC2_MUTEX_FLAG(1); // �ͷ�ˮ�ÿ���Ȩ
						timeout_count = 0;
						Set_Throttle_Value_2(MIN_CNT); 
					}
			
					if(timeout_count > 30) 
					{
						check_count = 0;
						timeout_count = 0;
						check_state = pressure_check_fail;
						SET_FOC2_MUTEX_FLAG(1); // �ͷ�ˮ�ÿ���Ȩ
					}
					
					
			uart_printf( 2 ,"check_state =%d  check_count = %d  timeout_count = %d perssure = %d  Throttle_Value_2 = %d \r\n", 
			            check_state,check_count,timeout_count,test_value,Throttle_Value_2);
				break;
			
				case pressure_check_sucessfull:
				// �����ﲻ��ֵҲ�У���Ϊ��ʼ�ĸ�ֵ�Ѿ�ͨ��У��
				break;
					
				case pressure_check_fail:
					
				//spray_system_cali_para.need_cali_flag = 1; // ��Ҫ����У׼����ѹ��
				break;
				
				default:
					
				break;
			} 
		}
		if(check_state == pressure_check_sucessfull)
			return pressure_check_sucessfull;
		else if(check_state == pressure_check_fail)
			return pressure_check_fail;
		else
		{
			return 0;
		}
}

uint8_t pressure1_check_process(void) // 10hz ����
{	
		#ifdef  DEBUG_FLOW_LEVEL
			static	 uint8_t check_state = pressure_check_unknow; 
	  #else
			static	 uint8_t check_state = pressure_check_min;
	  #endif
	  static uint16_t check_count = 0,timeout_count = 0;// 
	  uint16_t  test_value = 
	  test_value = get_press_value(Press_Sensor_0);
    if(spray_system_pressure_cali_para_1.need_cali_flag == 1)
		{
		    return pressure_check_fail;//��APPӦ����Ҫ����У׼ 
		}
		else
		{
				if(check_state != pressure_check_sucessfull && check_state != pressure_check_fail)
				{
					 #ifdef  DEBUG_FLOW_LEVEL	
					 if(FOC_1_STATUS.enable_flag == STOP_SPARY) // ����sensor id �ж�
					 {			 
						 SET_FOC1_MUTEX_FLAG(1); // �ͷ�ˮ�ÿ���Ȩ , ��Ҫ�����õ�������
						 check_state = pressure_check_unknow;
					 }
					 if(check_state == pressure_check_unknow 
							&& FOC_1_STATUS.enable_flag == START_SPARY
					    && FOC_1_STATUS.valid_spray == 1
							)//
					 {
						 check_state = pressure_check_min;
						 SET_FOC1_MUTEX_FLAG(0); // ��ȡˮ�ÿ���Ȩ
					 }
					 #endif
				}
				
			switch(check_state)
			{
			  case pressure_check_min:
				  Set_Throttle_Value_1(spray_system_pressure_cali_para_1.throttle_value_min);
				  if(get_press_value(Press_Sensor_0) >= WORKING_PRESSURE_MIN - 2*DEAD_ZONE
							&& get_press_value(Press_Sensor_0 ) <= WORKING_PRESSURE_MIN + 2*DEAD_ZONE )
						check_count++;
					else
					  timeout_count++;
					
					if(check_count > 5) // 1s
					{
					  check_count = 0;
						check_state = pressure_check_default;
						timeout_count = 0;
					}
			
					if(timeout_count > 50) //
					{
						check_count = 0;
						timeout_count = 0;
						check_state = pressure_check_fail;
						SET_FOC1_MUTEX_FLAG(1); // �ͷ�ˮ�ÿ���Ȩ 
					}
					

			uart_printf( 2 ,"check_state =%d  check_count = %d  timeout_count = %d perssure = %d  Throttle_Value_1 = %d \r\n", 
			            check_state,check_count,timeout_count,test_value,Throttle_Value_1);
				break;
				
				case pressure_check_default:
	
					Set_Throttle_Value_1(spray_system_pressure_cali_para_1.throttle_value_default); 
				 
				  if(get_press_value(Press_Sensor_0) >= WORKING_PRESSURE_DEFAULT - 2*DEAD_ZONE 
							&& get_press_value(Press_Sensor_0) <= WORKING_PRESSURE_DEFAULT + 2*DEAD_ZONE )
						check_count++;
					else
					  timeout_count++;
					
					if(check_count > 5) // 1s
					{
					  check_count = 0;
						check_state = pressure_check_max;
						timeout_count = 0;
					}
			
					if(timeout_count > 30) // 
					{
						check_count = 0;
						timeout_count = 0;
						check_state = pressure_check_fail;
						SET_FOC1_MUTEX_FLAG(1); // �ͷ�ˮ�ÿ���Ȩ
					}

			uart_printf( 2 ,"check_state =%d  check_count = %d  timeout_count = %d perssure = %d  Throttle_Value_1 = %d \r\n", 
			            check_state,check_count,timeout_count,test_value,Throttle_Value_1);
				break;
				
				case pressure_check_max:
						Set_Throttle_Value_1(spray_system_pressure_cali_para_1.throttle_value_max); 
				  if(get_press_value(Press_Sensor_0) >= WORKING_PRESSURE_MAX - 2*DEAD_ZONE 
							&& get_press_value(Press_Sensor_0) <= WORKING_PRESSURE_MAX + 2*DEAD_ZONE )
						check_count++;
					else
					  timeout_count++;
					
					if(check_count > 5) // 1s
					{
					  check_count = 0;
						check_state = pressure_check_sucessfull;
						SET_FOC1_MUTEX_FLAG(1); // �ͷ�ˮ�ÿ���Ȩ
						timeout_count = 0;
						Set_Throttle_Value_1(MIN_CNT); 
					}
			
					if(timeout_count > 30) 
					{
						check_count = 0;
						timeout_count = 0;
						check_state = pressure_check_fail;
						SET_FOC1_MUTEX_FLAG(1); // �ͷ�ˮ�ÿ���Ȩ
					}

			uart_printf( 2 ,"check_state =%d  check_count = %d  timeout_count = %d perssure = %d  Throttle_Value_1 = %d \r\n", 
			            check_state,check_count,timeout_count,test_value,Throttle_Value_1);
				break;
			
				case pressure_check_sucessfull:
					pressure1_check_complted = 1;
				// �����ﲻ��ֵҲ�У���Ϊ��ʼ�ĸ�ֵ�Ѿ�ͨ��У��
				break;
					
				case pressure_check_fail:
					
				//spray_system_cali_para.need_cali_flag = 1; // ��Ҫ����У׼����ѹ��
				break;
				
				default:
					
				break;
			} 
		}
		if(check_state == pressure_check_sucessfull)
			return pressure_check_sucessfull;
		else if(check_state == pressure_check_fail)
			return pressure_check_fail;
		else
		{
			return 0;
		}
}

#define pressure_cali_min       0 
#define pressure_cali_default      1 
#define pressure_cali_max          2
#define pressure_para_write_flash  3
#define pressure_cali_sucessfull   4
#define pressure_cali_fail         5  
#define pressure_cali_unknown      6 



void pressure2_calibration_process(void)// ѹ����У׼����
{
	#ifdef  DEBUG_FLOW_LEVEL
  static	uint8_t cali_step = pressure_cali_unknown; 
	#else
	static	uint8_t cali_step = pressure_cali_min;
	#endif

  uint16_t switch_value = 0;
	static uint8_t cali_fail_count = 0;
	if( cali_step != pressure_cali_sucessfull && flow_cali_enbale_flag == 0 
		  && pressure1_check_complted == 1) // �ڲ��궨�����Ƶ�ǰ����,������һ�����Լ���ɺ�
	{
			if( pressure2_check_process() == pressure_check_fail)
			{
					#ifdef  DEBUG_FLOW_LEVEL
					if(FOC_2_STATUS.enable_flag == STOP_SPARY) // ����sensor id �ж�	
					{
						cali_step = pressure_cali_unknown;
						SET_FOC2_MUTEX_FLAG(1); // �ͷ�ˮ�ÿ���Ȩ
					}
					if(cali_step == pressure_cali_unknown
						 && FOC_2_STATUS.enable_flag == START_SPARY
					   && FOC_2_STATUS.valid_spray == 1
						) 
					{
						cali_step = pressure_cali_min;
						SET_FOC2_MUTEX_FLAG(0); // ��ȡˮ�ÿ���Ȩ
					}
					#endif

				switch(cali_step)
				{
					case pressure_cali_min:
						switch_value = Set_FOC2_Value_By_Pressure1(DEFAULT_FOC_VALUE,WORKING_PRESSURE_MIN);	  
						if(switch_value == 0) // У׼ʧ��
						{
							cali_step = pressure_cali_fail; 
							SET_FOC2_MUTEX_FLAG(1); // �ͷ�ˮ�ÿ���Ȩ
						}
						else if (switch_value > MIN_CNT ) // У׼���
						{
							spray_system_pressure_cali_para_2.throttle_value_min  = switch_value; 
							cali_step = pressure_cali_default;
						}
						

					break;
					
					case pressure_cali_default:
						switch_value = Set_FOC2_Value_By_Pressure1(spray_system_pressure_cali_para_2.throttle_value_min,WORKING_PRESSURE_DEFAULT);
						if(switch_value == 0) // У׼ʧ��
						{
							cali_step = pressure_cali_fail; 
							SET_FOC2_MUTEX_FLAG(1); // �ͷ�ˮ�ÿ���Ȩ
						}
						else if (switch_value > MIN_CNT ) // У׼���
						{
							spray_system_pressure_cali_para_2.throttle_value_default  = switch_value; 
							cali_step = pressure_cali_max;
						}
					break;

					case pressure_cali_max:	
						switch_value = Set_FOC2_Value_By_Pressure1(spray_system_pressure_cali_para_2.throttle_value_default,WORKING_PRESSURE_MAX);
						if(switch_value == 0) // У׼ʧ��
						{
							cali_step = pressure_cali_fail; 
							SET_FOC2_MUTEX_FLAG(1); // �ͷ�ˮ�ÿ���Ȩ
						}
						else if (switch_value > MIN_CNT ) // У׼���
						{
							spray_system_pressure_cali_para_2.throttle_value_max  = switch_value; 
							cali_step = pressure_para_write_flash;
						}
					break;	
					
					case pressure_para_write_flash:		// У׼�ɹ� ���������app 
					  Set_Throttle_Value_2(MIN_CNT);  // У׼�ɹ� ֹͣת��
						spray_system_pressure_cali_para_2.save_flag = Data_Exists_Flag;
						spray_system_pressure_cali_para_2.need_cali_flag = 0;
						spray_system_pressure_cali_para_2.pressure_user = 0xAA;// for test
						cali_pressure2_para_write_flash((uint8_t*)&spray_system_pressure_cali_para_2,sizeof(spray_system_pressure_cali_para_t));
												
						Set_Spary_Limit_Value(FOC_2,MAX_SPRAY_ID,spray_system_pressure_cali_para_2.throttle_value_max);	
				    Set_Spary_Limit_Value(FOC_2,MIN_SPRAY_ID,spray_system_pressure_cali_para_2.throttle_value_min);		
		
						cali_step = pressure_cali_sucessfull;
						SET_FOC2_MUTEX_FLAG(1); // �ͷ�ˮ�ÿ���Ȩ
						uart_printf( 2, "************************pressure_para_write_flash****************\r\n" );
						// �������д����
					break;
					
					case pressure_cali_sucessfull:		// У׼�ɹ� ���������app 
						 // ִ�в���
					break;
					
					case pressure_cali_fail:	// У׼ʧ�� ���������app, ��ֹУ׼����APP�����
						cali_fail_count++;
					 if(cali_fail_count<3)
					 {
							cali_step = pressure_cali_min;
					 }
					 else
					 {
						 cali_fail_count = 3;
						 // �������ˮ�õ�ָ��
					 }
					break;
					
					default:
						
					break;
				}	
			}
  }
}

void pressure1_calibration_process(void)// ѹ����У׼����
{
	#ifdef  DEBUG_FLOW_LEVEL
  static	uint8_t cali_step = pressure_cali_unknown; 
	#else
	static	uint8_t cali_step = pressure_cali_min;
	#endif
  uint16_t switch_value = 0;
	static uint8_t cali_fail_count = 0;
	if( cali_step != pressure_cali_sucessfull && flow_cali_enbale_flag == 0) // �ڲ��궨�����Ƶ�ǰ����
	{
			if( pressure1_check_process() == pressure_check_fail)
			{
					#ifdef  DEBUG_FLOW_LEVEL
					if(FOC_1_STATUS.enable_flag == STOP_SPARY) // ����sensor id �ж�	
					{
						cali_step = pressure_cali_unknown;
						SET_FOC1_MUTEX_FLAG(1); // �ͷ�ˮ�ÿ���Ȩ
					}
					//FOC_1_STATUS.valid_spray  = 1;
					if(cali_step == pressure_cali_unknown
						 && FOC_1_STATUS.enable_flag == START_SPARY
					   && FOC_1_STATUS.valid_spray == 1
				)		
					{
						cali_step = pressure_cali_min;
						SET_FOC1_MUTEX_FLAG(0); // ��ȡˮ�ÿ���Ȩ
					}
					#endif
			 
				
				switch(cali_step)
				{
					case pressure_cali_min:
						switch_value = Set_FOC1_Value_By_Pressure0(DEFAULT_FOC_VALUE,WORKING_PRESSURE_MIN);	  
						if(switch_value == 0) // У׼ʧ��
						{
							cali_step = pressure_cali_fail; 
							SET_FOC1_MUTEX_FLAG(1); // �ͷ�ˮ�ÿ���Ȩ
						}
						else if (switch_value > MIN_CNT ) // У׼���
						{
							spray_system_pressure_cali_para_1.throttle_value_min  = switch_value; 
							cali_step = pressure_cali_default;
						}
					break;
					
					case pressure_cali_default:
						switch_value = Set_FOC1_Value_By_Pressure0(spray_system_pressure_cali_para_1.throttle_value_min,WORKING_PRESSURE_DEFAULT);
						if(switch_value == 0) // У׼ʧ��
						{
							cali_step = pressure_cali_fail; 
							SET_FOC1_MUTEX_FLAG(1); // �ͷ�ˮ�ÿ���Ȩ
						}
						else if (switch_value > MIN_CNT ) // У׼���
						{
							spray_system_pressure_cali_para_1.throttle_value_default  = switch_value; 
							cali_step = pressure_cali_max;
						}
					break;

					case pressure_cali_max:	
						switch_value = Set_FOC1_Value_By_Pressure0(spray_system_pressure_cali_para_1.throttle_value_default,WORKING_PRESSURE_MAX);
						if(switch_value == 0) // У׼ʧ��
						{
							cali_step = pressure_cali_fail; 
							SET_FOC1_MUTEX_FLAG(1); // �ͷ�ˮ�ÿ���Ȩ
						}
						else if (switch_value > MIN_CNT ) // У׼���
						{
							spray_system_pressure_cali_para_1.throttle_value_max  = switch_value; 
							cali_step = pressure_para_write_flash;
						}
					break;	
					
					case pressure_para_write_flash:		// У׼�ɹ� ���������app 
					
						Set_Throttle_Value_1(MIN_CNT); 
						spray_system_pressure_cali_para_1.save_flag = Data_Exists_Flag;
						spray_system_pressure_cali_para_1.need_cali_flag = 0;
						spray_system_pressure_cali_para_1.pressure_user = 0xAA;// for test
						cali_pressure1_para_write_flash((uint8_t*)&spray_system_pressure_cali_para_1,sizeof(spray_system_pressure_cali_para_t));
												
						Set_Spary_Limit_Value(FOC_1,MAX_SPRAY_ID,spray_system_pressure_cali_para_1.throttle_value_max);	
				    Set_Spary_Limit_Value(FOC_1,MIN_SPRAY_ID,spray_system_pressure_cali_para_1.throttle_value_min);		
							
						cali_step = pressure_cali_sucessfull;
					  pressure1_check_complted = 1;
						SET_FOC1_MUTEX_FLAG(1); // �ͷ�ˮ�ÿ���Ȩ
						uart_printf( 2, "************************pressure_para_write_flash****************\r\n" );
						// �������д����
					break;
					
					case pressure_cali_sucessfull:		// У׼�ɹ� ���������app 
						 // ִ�в���
					  
					break;
					
					case pressure_cali_fail:	// У׼ʧ�� ���������app, ��ֹУ׼����APP�����
						cali_fail_count++;
					 if(cali_fail_count<3)
					 {
							cali_step = pressure_cali_min;
					 }
					 else
					 {
						 cali_fail_count = 3;
						 //���ˮ��ѹ���궨�Ļ����־λ
						 
						 // �������ˮ�õ�ָ��
					 }
					break;
					
					default:
						
					break;
				}	
			}
  }
}

/********************************************
����Ƿ���ҩ���߼� 
*******************************************/


uint8_t pump1_state = Air_In_The_Pipe,pump2_state = Air_In_The_Pipe;
uint8_t get_pump1_state(void)
{
  return pump1_state;
}

uint8_t get_pump2_state(void)
{
  return pump2_state;
}

uint8_t MedeRunoutFlag = 0; // ��ʼ��Ϊ��ҩ
uint8_t remaining_capacity_detection( void )// 100HZ 
{
	uint8_t res = 0; 
	static uint16_t count_detect_1 = 0; 
	static uint16_t count_detect_2 = 0; 
	static uint16_t count_detect_3 = 0; 
	static uint16_t count_detect_4 = 0; 
	if( (get_foc_throttle_value(FOC_1) >= (Get_Spary_Limit_Value(FOC_1,MIN_SPRAY_ID) ) && get_cali_flow_freq() <= 1)
			 || (get_foc_throttle_value(FOC_2) >= (Get_Spary_Limit_Value(FOC_2,MIN_SPRAY_ID) ) && get_cali_flow_freq() <= 1))
	{
		count_detect_1++;
		count_detect_2 = 0;
		if(count_detect_1 >= 500)
		{
			count_detect_1 = 500;
		  if(get_capacity_flag_1L() == 1)//���Ӵ��������ô˷����ж���ҩ
			{
				set_MedeRunoutFlag(1);
			}
			pump1_state = Air_In_The_Pipe;
			pump2_state = Air_In_The_Pipe;
		}
	}
  else if((get_foc_throttle_value(FOC_1) >= (Get_Spary_Limit_Value(FOC_1,MIN_SPRAY_ID)) && get_cali_flow_freq() > 5)
	     || (get_foc_throttle_value(FOC_2) >= (Get_Spary_Limit_Value(FOC_2,MIN_SPRAY_ID)) && get_cali_flow_freq() > 5))
	{
		 count_detect_1 = 0;
		 if(FOC_1_STATUS.valid_spray == 1)// �ɻ���ɺ�ѹ���ж�
		 {
				if( (get_foc_throttle_value(FOC_1) > (Get_Spary_Limit_Value(FOC_1,MIN_SPRAY_ID)) && get_press_value(Press_Sensor_0) < 840)//adc  mv
		          || (get_foc_throttle_value(FOC_2) > (Get_Spary_Limit_Value(FOC_2,MIN_SPRAY_ID) ) && get_press_value(Press_Sensor_1) < 840))
				{
					count_detect_3++;
					count_detect_4 = 0;
					if(count_detect_3 >= 500)
					{
						count_detect_3 = 500;
						pump1_state = Air_In_The_Pipe;
			      pump2_state = Air_In_The_Pipe;
					}
				}
				else if((get_foc_throttle_value(FOC_1) > (Get_Spary_Limit_Value(FOC_1,MIN_SPRAY_ID)) && get_press_value(Press_Sensor_0) > 940)// adc :mv
						 || (get_foc_throttle_value(FOC_2) > (Get_Spary_Limit_Value(FOC_2,MIN_SPRAY_ID)) && get_press_value(Press_Sensor_1) > 940))
				{
					count_detect_3 = 0;
					count_detect_4++;
					if(count_detect_4 > 300)
					{	
						count_detect_4 = 300;
						pump1_state = Pump_Normal;
						pump2_state = Pump_Normal;
					}
				}
				else
				{
				  count_detect_3 = 0;
					count_detect_4 = 0;
				}
		  //uart_printf( 2, "count_detect_3  = %d  count_detect_4 = %d \r\n", count_detect_3, count_detect_4);	
			}
			else// ��غ��������ж�
			{
					count_detect_2++;
				  count_detect_3 = 0;
				  count_detect_4 = 0;
					if(count_detect_2 > 200)
					{	
						count_detect_2 = 200;
						pump1_state = Pump_Normal;
						pump2_state = Pump_Normal;
					}	
			}		
			//uart_printf( 2, "count_detect_1  = %d  count_detect_2 = %d \r\n", count_detect_1, count_detect_2);	
			//uart_printf( 2, "pump1_state  = %d  pump2_state = %d \r\n", pump1_state, pump2_state);
	}
	
	return res;
}




uint8_t get_MedeRunoutFlag(void)
{
   return MedeRunoutFlag;
}
 
void set_MedeRunoutFlag(uint8_t state)
{
    MedeRunoutFlag = state;
}

/****************************************************************************
����ϵͳ�����������ж�

ѹ������ϣ�
1.ˮ��û������������£�ѹ����������ֵ����1200mv(���ѹ���¿϶�����ˮ������)
  ����������
2.����ֵ������Сֵ������ѹ��ֵС������������ ����·�п�������ѹ������������
3.����ֵ������Сֵ����ѹ��ֵ����ˮ�ö�������ѹ������������
4.�κ������ѹ����������ֵС��400mv,���ߴ���4000mvֱ�ӱ�����

��������ϣ�
1.ˮ��û�й������������м����������ƴ���û�����������Ȳ��ӣ�
2.ˮ�ù���������û����������ֱ�ӻᱨ��ҩ
3.������������Χ��ʱû����̫�쳣�ģ��������䣩

****************************************************************************/
//uint8_t pump1_fault1_count,pump1_fault2_count,pump1_fault3_count;
//uint8_t pump2_fault1_count,pump2_fault2_count,pump2_fault3_count;
//void Spray_system_fault_diagnosis(void)
//{
//    if(get_press_value(Press_Sensor_0) > 4000 
//		   ||get_press_value(Press_Sensor_0) < 400)
//		{
//		   // ˮ��1����
//			pump1_fault3_count++;
//			if(pump1_fault3_count > 20)//2s
//			  pump1_fault_state = Sensor_Failure;
//		}
//		else if(get_foc_throttle_value(FOC_1) == MIN_CNT && get_press_value(Press_Sensor_0) > 1200)
//		{
//			pump1_fault3_count++;
//		  if(pump1_fault3_count > 20)//2s
//			  pump1_fault_state = Sensor_Failure;
//		}
//		else if(get_foc_throttle_value(FOC_1) >= Get_Spary_Limit_Value(FOC_1,MIN_SPRAY_ID) 
//			      && get_press_value(Press_Sensor_0) < 800)
//		{
//			pump1_fault2_count++;
//			if(pump1_fault2_count > 20)
//		   pump1_fault_state = Air_In_The_Pipe;
//		}
//		else if(get_foc_throttle_value(FOC_1) >= Get_Spary_Limit_Value(FOC_1,MIN_SPRAY_ID) 
//			      && get_press_value(Press_Sensor_0) > 2500)
//		{
//			pump1_fault1_count++;
//			if(pump1_fault1_count > 20)
//		   pump1_fault_state = Pipe_Blockage;
//		}
//		else
//		{
//			pump1_fault1_count = 0;
//			pump1_fault2_count = 0;
//			pump1_fault3_count = 0;
//		  pump1_fault_state = Pump_Normal;
//		}
//		
//		if(get_press_value(Press_Sensor_1) > 4000 
//		   ||get_press_value(Press_Sensor_1) < 400)
//		{
//		   // ˮ��1����
//			pump2_fault3_count++;
//			if(pump2_fault3_count > 20)//2s
//			  pump2_fault_state = Sensor_Failure;
//		}
//		else if(get_foc_throttle_value(FOC_2) == MIN_CNT && get_press_value(Press_Sensor_1) > 1200)
//		{
//			pump2_fault3_count++;
//		  if(pump2_fault3_count > 20)//2s
//			  pump2_fault_state = Sensor_Failure;
//		}
//		else if(get_foc_throttle_value(FOC_2) >= Get_Spary_Limit_Value(FOC_2,MIN_SPRAY_ID) 
//			      && get_press_value(Press_Sensor_1) < 800)
//		{
//			pump2_fault2_count++;
//			if(pump2_fault2_count > 20)
//		   pump2_fault_state = Air_In_The_Pipe;
//		}
//		else if(get_foc_throttle_value(FOC_2) >= Get_Spary_Limit_Value(FOC_2,MIN_SPRAY_ID) 
//			      && get_press_value(Press_Sensor_1) > 2500)
//		{
//			pump2_fault1_count++;
//			if(pump2_fault1_count > 20)
//		   pump2_fault_state = Pipe_Blockage;
//		}
//		else
//		{
//			pump2_fault1_count = 0;
//			pump2_fault2_count = 0;
//			pump2_fault3_count = 0;
//		  pump2_fault_state = Pump_Normal;
//		}
//}










