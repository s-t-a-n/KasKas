import streamlit as st  # web development
import streamlit_authenticator as stauth
import numpy as np 

import pandas as pd  # read csv, df manipulation
import time  # to simulate a real time data, time loop
from datetime import timedelta
from datetime import datetime
import plotly.express as px  # interactive charts
from picamera2 import Picamera2
import cv2
import resource
import sys

st.set_page_config(page_title="KasKas !", page_icon="🌱", layout="wide")


# def limit_memory(maxsize_MB: int):
#     print(f"setting memory limit to {maxsize_MB}MB")
#     print(f"setting memory limit to {int(maxsize_MB * 1e6)}B")
#     soft, hard = resource.getrlimit(resource.RLIMIT_AS)
#     resource.setrlimit(resource.RLIMIT_AS, (int(maxsize_MB * 1e6), hard))
# limit_memory(1600)



# ripped from https://stackoverflow.com/a/58314952
def available_memory() -> int:
    with open("/proc/meminfo", "r") as mem:
        free_memory = 0
        for i in mem:
            sline = i.split()
            if str(sline[0]) in ("MemFree:", "Buffers:", "Cached:"):
                free_memory += int(sline[1])
    return free_memory * 1024 # bytes


def memory_limit(percentage: int):
    assert 0 < percentage <= 100
    soft, hard = resource.getrlimit(resource.RLIMIT_AS)
    available_memory_MB = available_memory() / 1024 / 1024
    print(
        f"setting memory limit to {percentage}% of {available_memory_MB}MB"
    )
    new_soft = int(available_memory() * (percentage / 100.0)) 
    resource.setrlimit(
        resource.RLIMIT_AS,
        (
            new_soft,
            hard,
        ),
    )
    print(f"Set memory limit to {new_soft} (was {soft})")

def memory(percentage:int=80):
    def decorator(function):
        def wrapper(*args, **kwargs):
            memory_limit(percentage)
            try:
                return function(*args, **kwargs)
            except MemoryError:
                mem = available_memory() / 1024 / 1024 / 1024
                print('Remain: %.2f GB' % mem)
                sys.stderr.write('\n\nERROR: Memory Exception\n')
                sys.exit(1)
        return wrapper
    return decorator


@st.cache_resource
def get_camera() -> Picamera2:
    picam2 = Picamera2()
# >>> pprint(picam2.sensor_modes)
# [{'bit_depth': 10,
#   'crop_limits': (16, 0, 2560, 1920),
#   'exposure_limits': (134, 1103219, None),
#   'format': SGBRG10_CSI2P,
#   'fps': 58.92,
#   'size': (640, 480),
#   'unpacked': 'SGBRG10'},
#  {'bit_depth': 10,
#   'crop_limits': (0, 0, 2592, 1944),
#   'exposure_limits': (92, 760636, None),
#   'format': SGBRG10_CSI2P,
#   'fps': 43.25,
#   'size': (1296, 972),
#   'unpacked': 'SGBRG10'},
#  {'bit_depth': 10,
#   'crop_limits': (348, 434, 1928, 1080),
#   'exposure_limits': (118, 969249, None),
#   'format': SGBRG10_CSI2P,
#   'fps': 30.62,
#   'size': (1920, 1080),
#   'unpacked': 'SGBRG10'},
#  {'bit_depth': 10,
#   'crop_limits': (0, 0, 2592, 1944),
#   'exposure_limits': (130, 1064891, None),
#   'format': SGBRG10_CSI2P,
#   'fps': 15.63,
#   'size': (2592, 1944),
#   'unpacked': 'SGBRG10'}]
    mode = picam2.sensor_modes[3]
    
    picam2.configure(
        picam2.create_preview_configuration(
            main={"format": "XRGB8888", "size": (640, 480)},
            sensor={'output_size': mode['size'], 'bit_depth': mode['bit_depth']}
        )
    )
    if picam2.started:
        picam2.stop()
    picam2.start()
    return picam2


@st.cache_resource(ttl=timedelta(seconds=10))
def get_next_frame() -> np.ndarray:
    next_frame = get_camera().capture_array()
    
    # Get current date and time  
    date_time = str(datetime.now())
    
    font = cv2.FONT_HERSHEY_SIMPLEX 
    x = next_frame.shape[1]
    y = next_frame.shape[0]
    origin = (x- len(date_time) * 10, 20);
    fontscale = 0.5
    color_bgr = (0, 51, 153)
    thickness = 1



    # write the date time in the video frame
    next_frame = cv2.putText(next_frame, date_time,origin,font, fontscale,color_bgr, thickness, cv2.LINE_4)
    return next_frame


@st.cache_resource(ttl=timedelta(seconds=1))
def get_next_dataframe() -> pd.DataFrame:
    # return pd.read_csv("/home/stan/projects/KasKas/src/python/kaskas_data.csv", low_memory=True)
    return pd.read_csv("/mnt/USB/kaskas_data.csv", low_memory=True)


def load_page(display_interval: int, frames_per_visit:int, authenticator: stauth.Authenticate):
    print("Client connected")
    
    header_left, title_col, header_right = st.columns(3)
    with title_col:
        st.title("KasKas !")
        st.subheader("accept no substitutes")

    with header_left:
        with st.popover("API"):
            # Initialize chat history
            if "messages" not in st.session_state:
                st.session_state.messages = []

            # Display chat messages from history on app rerun
            for message in st.session_state.messages:
                with st.chat_message(message["role"]):
                    st.markdown(message["content"])

            # React to user input
            if prompt := st.chat_input("What is up?"):
                # Display user message in chat message container
                st.chat_message("user").markdown(prompt)
                # Add user message to chat history
                st.session_state.messages.append({"role": "user", "content": prompt})

                response = f"Response: {prompt}"
                # Display assistant response in chat message container
                with st.chat_message("assistant"):
                    st.markdown(response)
                # Add assistant response to chat history
                st.session_state.messages.append({"role": "assistant", "content": response})
                # messages = st.container()
                # if prompt := st.chat_input("Go ahead! Talk to KasKas:"):
                #     messages.chat_message("user").write(prompt)
                #     messages.chat_message("assistant").write(f"Echo: {prompt}")

    with header_right:
        if st.session_state["authentication_status"]:
            authenticator.logout()

    # creating a single-element container.
    placeholder = st.empty()

    # To combat memory leakage, run for X times, then quit
    for i in range(frames_per_visit):
        df = get_next_dataframe()

        # creating KPIs
        element_surface_temp = np.mean(df["HEATING_SURFACE_TEMP"].iloc[-2:])
        climate_setpoint = np.mean(df["HEATING_SETPOINT"].iloc[-1:])
        climate_temp = np.mean(df["CLIMATE_TEMP"].iloc[-2:])
        climate_humidity = np.mean(df["CLIMATE_HUMIDITY"].iloc[-2:])
        soil_moisture = np.mean(df["SOIL_MOISTURE"].iloc[-2:])

        avg_element_surface_temp = np.mean(df["HEATING_SURFACE_TEMP"].iloc[-20:])
        avg_climate_temp = np.mean(df["CLIMATE_TEMP"].iloc[-20:])
        avg_climate_humidity = np.mean(df["CLIMATE_HUMIDITY"].iloc[-20:])
        avg_soil_moisture = np.mean(df["SOIL_MOISTURE"].iloc[-20:])

        with placeholder.container():
            # 2024-06-19 19:13:09.467814
            time_since_last_sample = datetime.now() - datetime.strptime(
                df["TIMESTAMP"].iloc[-1], "%Y-%m-%d %H:%M:%S.%f"
            )
            if time_since_last_sample.total_seconds() > 60:
                st.warning(f"Datacollection failure. You are watching outdated data.")

            
            left_image, webcam_col, water_panel = st.columns(3)
            with webcam_col:
                # todo: some memory leaking is happening here
                # likely the array is kept referenced within st.image
                # on gh,
                if get_camera().started:
                    last_frame = get_next_frame()
                    st.image(last_frame, caption="Livefeed")
            with left_image:
                st.image("./Nietzsche_metPistool.jpg", width=420, caption="nature or notsure")
            # with water_panel:
            #     # st.image("./pexels-photo-28924.jpg", width=500, caption="Nature")
            #     with st.container():
            #         left_column, middle_column, right_column = st.columns(3)
                    
            #         def start_injection():
            #             print("starting injection")
                     
            #         with left_column:
            #           pass
            #         with right_column:
            #             number = st.number_input("Amount in milliliter",value=100, max_value=500)
                        
            #             if st.button("Water plants",on_click=start_injection):
            #                 pass
            #             else:
            #                 pass
            #     pass
                    

            # create three columns
            kpi1, kpi2 = st.columns(2)

            # fill in those three columns with respective metrics or KPIs
            kpi1.metric(
                label="Element surface temperature",
                value=round(element_surface_temp, 2),
                delta=round(element_surface_temp - avg_element_surface_temp, 2),
            )
            kpi2.metric(
                label="Climate temperature",
                value=round(climate_temp, 2),
                delta=round(climate_temp - avg_climate_temp, 2),
            )

            fig_col1, fig_col2 = st.columns(2)
            with fig_col1:
                st.markdown("### Element surface temperature")
                fig1 = px.scatter(data_frame=df, y="HEATING_SURFACE_TEMP", x="TIMESTAMP")
                st.write(fig1)
            with fig_col2:
                st.markdown("### Climate temperature")
                fig2 = px.line(
                    data_frame=df,
                    y=["CLIMATE_TEMP", "HEATING_SETPOINT", "AMBIENT_TEMP"],
                    x="TIMESTAMP",
                )
                st.write(fig2)

            kpi3, kpi4 = st.columns(2)
            kpi3.metric(
                label="Climate humidity",
                value=round(climate_humidity, 2),
                delta=round(climate_humidity - avg_climate_humidity, 2),
            )
            kpi4.metric(
                label="Soil moisture",
                value=round(soil_moisture, 2),
                delta=round(soil_moisture - avg_soil_moisture, 2),
            )

            fig_col3, fig_col4 = st.columns(2)
            with fig_col3:
                st.markdown("### Climate  humidity")
                fig3 = px.area(
                    data_frame=df, y=["CLIMATE_HUMIDITY", "CLIMATE_FAN"], x="TIMESTAMP", range_y=[0,100]
                )
                st.write(fig3)
            with fig_col4:
                st.markdown("### Soil moisture")
                fig4 = px.line(
                    data_frame=df,
                    y=["SOIL_MOISTURE", "SOIL_MOISTURE_SETPOINT"],
                    x="TIMESTAMP",
                    range_y=[0,100]
                )
                st.write(fig4)

            st.markdown("### Detailed Data View")
            st.dataframe(df, use_container_width=True)
            time.sleep(display_interval)



# https://github.com/streamlit/streamlit/issues/6354
# memory_limit(percentage=80)


import yaml
from yaml.loader import SafeLoader

with open('auth.yml') as file:
    config = yaml.load(file, Loader=SafeLoader)

authenticator = stauth.Authenticate(
    config['credentials'],
    config['cookie']['name'],
    config['cookie']['key'],
    config['cookie']['expiry_days'],
    config['pre-authorized']
)

if not st.session_state["authentication_status"]:
    name, authentication_status, username = authenticator.login()


# try:
#     email_of_registered_user, username_of_registered_user, name_of_registered_user = authenticator.register_user(pre_authorization=False)
#     if email_of_registered_user:
#         st.success('User registered successfully')
# except Exception as e:
#     st.error(e)

# if st.session_state["authentication_status"]:
#     try:
#         if authenticator.reset_password(st.session_state["username"]):
#             st.success('Password modified successfully')
#     except Exception as e:
#         st.error(e)

with open('auth.yml', 'w') as file:
    yaml.dump(config, file, default_flow_style=False)

# if st.session_state["authentication_status"]:
#     try:
#         if authenticator.update_user_details(st.session_state["username"]):
#             st.success('Entries updated successfully')
#     except Exception as e:
#         st.error(e)

if st.session_state["authentication_status"]:
    # authenticator.logout()
    # st.write(f'Welcome *{st.session_state["name"]}*')
    load_page(display_interval=10,frames_per_visit=300 ,authenticator=authenticator)
    st.rerun()
elif st.session_state["authentication_status"] is False:
    st.error('Username/password is incorrect')
elif st.session_state["authentication_status"] is None:
    st.warning('Please enter your username and password')


# load_page(display_interval=10,frames_per_visit=30)

# try:
#     load_page(display_interval=10,frames_per_visit=30)
# except MemoryError:
#     print("Caught memory error")
# except:
#     print("Caught unknown exception")

print("End of session. Scheduling rerun.")
