import streamlit as st  # web development
import numpy as np  # np mean, np random
import pandas as pd  # read csv, df manipulation
import time  # to simulate a real time data, time loop
from datetime import timedelta
from datetime import datetime
import plotly.express as px  # interactive charts
from picamera2 import Picamera2
import resource
import sys

# def limit_memory(maxsize_MB: int):
#     print(f"setting memory limit to {maxsize_MB}MB")
#     print(f"setting memory limit to {int(maxsize_MB * 1e6)}B")
#     soft, hard = resource.getrlimit(resource.RLIMIT_AS)
#     resource.setrlimit(resource.RLIMIT_AS, (int(maxsize_MB * 1e6), hard))
# limit_memory(1600)




def get_memory_kB():
    with open("/proc/meminfo", "r") as mem:
        free_memory = 0
        for i in mem:
            sline = i.split()
            if str(sline[0]) in ("MemFree:", "Buffers:", "Cached:"):
                free_memory += int(sline[1])
    return free_memory  # KiB


def memory_limit(percentage_of_free_memory: int):
    assert 0 < percentage_of_free_memory <= 100
    soft, hard = resource.getrlimit(resource.RLIMIT_DATA)
    # Convert KiB to bytes, and divide in two to half
    available_memory_MB = get_memory_kB() / 1024
    print(
        f"setting memory limit to {percentage_of_free_memory}% of {available_memory_MB}MB"
    )
    resource.setrlimit(
        resource.RLIMIT_DATA,
        (
            int(available_memory_MB * 1024 * 1024 * percentage_of_free_memory / 100),
            hard,
        ),
    )


@st.cache_resource
def get_camera() -> Picamera2:
    picam2 = Picamera2()
    picam2.configure(
        picam2.create_preview_configuration(
            main={"format": "XRGB8888", "size": (640, 480)}
        )
    )
    if picam2.started:
        picam2.stop()
    picam2.start()
    return picam2


@st.cache_resource(ttl=timedelta(seconds=1))
def get_next_picture() -> np.ndarray:
    return get_camera().capture_array()


@st.cache_resource(ttl=timedelta(seconds=1))
def get_next_dataframe() -> pd.DataFrame:
    # return pd.read_csv("/home/stan/projects/KasKas/src/python/kaskas_data.csv", low_memory=True)
    return pd.read_csv("/mnt/USB/kaskas_data.csv", low_memory=True)


def load_page(display_interval: int, frames_per_visit:int):
    print("Client connected")
    
    st.set_page_config(page_title="KasKas !", page_icon="🌱", layout="wide")

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

            if get_camera().started:
                left_image, webcam_col, right_image = st.columns(3)
                with webcam_col:
                    # todo: some memory leaking is happening here
                    # likely the array is kept referenced within st.image
                    # on gh,
                    last_frame = get_next_picture()
                    ab = st.image(last_frame, caption="Livefeed")
                with left_image:
                    st.image("./Nietzsche_metPistool.jpg", width=420, caption="Notsure")
                with right_image:
                    st.image("./pexels-photo-28924.jpg", width=500, caption="Nature")

            # create three columns
            kpi1, kpi2 = st.columns(2)

            # fill in those three columns with respective metrics or KPIs
            kpi1.metric(
                label="Element surface temperature",
                value=round(element_surface_temp, 2),
                delta=round(abs(avg_element_surface_temp - element_surface_temp), 2),
            )
            kpi2.metric(
                label="Climate temperature",
                value=round(climate_temp, 2),
                delta=round(abs(avg_climate_temp - climate_temp), 2),
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
                delta=round(abs(avg_climate_humidity - climate_humidity), 2),
            )
            kpi4.metric(
                label="Soil moisture",
                value=round(soil_moisture, 2),
                delta=round(abs(avg_soil_moisture - soil_moisture), 2),
            )

            fig_col3, fig_col4 = st.columns(2)
            with fig_col3:
                st.markdown("### Climate  humidity")
                fig3 = px.area(
                    data_frame=df, y=["CLIMATE_HUMIDITY", "CLIMATE_FAN"], x="TIMESTAMP"
                )
                st.write(fig3)
            with fig_col4:
                st.markdown("### Soil moisture")
                fig4 = px.line(
                    data_frame=df,
                    y=["SOIL_MOISTURE", "SOIL_MOISTURE_SETPOINT"],
                    x="TIMESTAMP",
                )
                st.write(fig4)

            st.markdown("### Detailed Data View")
            st.dataframe(df, use_container_width=True)
            time.sleep(display_interval)



# https://github.com/streamlit/streamlit/issues/6354
memory_limit(percentage_of_free_memory=80)
try:
    load_page(display_interval=10,frames_per_visit=30)
except MemoryError:
    print("Caught memory error")
except:
    print("Caught unknown exception")

print("End of session. Scheduling rerun.")
st.rerun()
