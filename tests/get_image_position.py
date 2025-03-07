import matplotlib.pyplot as plt
import matplotlib.image as mpimg

# Load your image
# img = mpimg.imread('tests/snapshots/stax/test_get_addr_confirm_ok/00001.png')
# img = mpimg.imread('tests/snapshots/flex/test_get_addr_confirm_ok/00001.png')
# img = mpimg.imread('tests/snapshots/stax/test_sign_msg_short_ok/00004.png')
# img = mpimg.imread('tests/snapshots/flex/test_sign_msg_long/00001.png')
img = mpimg.imread(
    'tests/snapshots/flex/test_toggle_contract_data_0/00002.png')

# Display the image
fig, ax = plt.subplots()
ax.imshow(img)

# Function to print mouse position when clicked


def on_click(event):
    if event.inaxes:
        x, y = event.xdata, event.ydata
        print(f"Clicked position: x={x}, y={y}")


# Connect the click event to the function
fig.canvas.mpl_connect('button_press_event', on_click)

plt.show()
