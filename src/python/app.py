import streamlit as st # web development
import numpy as np # np mean, np random 
import pandas as pd # read csv, df manipulation
import time # to simulate a real time data, time loop 
import plotly.express as px # interactive charts 


# read csv from a github repo
df = pd.read_csv("/home/stan/projects/KasKas/src/python/kaskas_data.cv")


st.set_page_config(
    page_title = 'KasKas ! accept no substitutes',
    page_icon = '✅',
    layout = 'wide'
)

# dashboard title

st.title("KasKas ! accept no substitutes")

# top-level filters 

# job_filter = st.selectbox("Select the Job", pd.unique(df['job']))


# creating a single-element container.
placeholder = st.empty()

# dataframe filter 

# df = df[df['job']==job_filter]

# near real-time / live feed simulation 

# for seconds in range(200):
while True: 
    #  TIMESTAMP,CLIMATE_TEMP,HEATER_SURFACE_TEMP,OUTSIDE_TEMP,CLIMATE_HUMIDITY,SOIL_MOISTURE,

    df = pd.read_csv("/home/stan/projects/KasKas/src/python/kaskas_data.csv")

    # creating KPIs 
    avg_element_surface_temp = np.mean(df['HEATER_SURFACE_TEMP'].iloc[-10:]) 
    avg_climate_temp = np.mean(df['CLIMATE_TEMP'].iloc[-10:]) 
    avg_climate_humidity = np.mean(df['CLIMATE_HUMIDITY'].iloc[-10:]) 
    avg_soil_moisture = np.mean(df['SOIL_MOISTURE'].iloc[-10:]) 


    with placeholder.container():
        # create three columns
        kpi1, kpi2 = st.columns(2)

        # fill in those three columns with respective metrics or KPIs 
        kpi1.metric(label="Element surface temperature", value=round(avg_element_surface_temp, 2), delta= round(avg_element_surface_temp, 2) - 2)
        kpi2.metric(label="Climate temperature", value=round(avg_climate_temp, 2), delta= round(avg_climate_temp, 2) - 2)
        
        fig_col1, fig_col2 = st.columns(2)
        with fig_col1:
            st.markdown("### Element surface temperature")
            fig1 = px.line(data_frame = df, y = 'HEATER_SURFACE_TEMP', x = 'TIMESTAMP')
            st.write(fig1)
        with fig_col2:
            st.markdown("### Climate temperature")
            fig2 = px.line(data_frame=df, y = 'CLIMATE_TEMP', x = 'TIMESTAMP')
            st.write(fig2)
        
        
        kpi3, kpi4 = st.columns(2)
        kpi3.metric(label="Climate humidity", value=round(avg_climate_humidity, 2), delta= round(avg_climate_humidity, 2) - 2)
        kpi4.metric(label="Soil moisture", value=round(avg_soil_moisture, 2), delta= round(avg_soil_moisture, 2) - 2)


        fig_col3, fig_col4 = st.columns(2)
        with fig_col3:
            st.markdown("### Climate  humidity")
            fig3 = px.density_heatmap(data_frame = df, y = 'CLIMATE_HUMIDITY', x = 'TIMESTAMP')
            st.write(fig3)
        with fig_col4:
            st.markdown("### Soil moisture")
            fig4 = px.line(data_frame = df, y = 'SOIL_MOISTURE', x = 'TIMESTAMP')
            st.write(fig4)

        st.markdown("### Detailed Data View")
        st.dataframe(df, use_container_width=True)
        time.sleep(1)
    #placeholder.empty()


