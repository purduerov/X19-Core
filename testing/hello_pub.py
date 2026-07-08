from core.python.core.messaging import Publisher, Subscriber

def test_pub_sub():
    # Define a simple callback function for the subscriber
    def callback(message):
        print(f"Received message: {message}")

    # Create a publisher and subscriber
    publisher = Publisher(address='tcp://localhost:5555', topic='test')
    subscriber = Subscriber(address='tcp://localhost:5555', topic='test', message_type=SensorData, callback=callback)   
    
