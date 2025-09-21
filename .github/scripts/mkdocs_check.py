from selenium import webdriver
from selenium.webdriver.common.by import By
from selenium.webdriver.support.ui import WebDriverWait
from selenium.webdriver.support import expected_conditions as EC
import requests

# Set up the Chrome WebDriver
options = webdriver.ChromeOptions()
options.add_argument('--headless')
options.add_argument('--no-sandbox')
options.add_argument('--disable-dev-shm-usage')


url = 'https://trapexit.github.io/mergerfs'
print(f"*** Checking {url} ***")

driver = webdriver.Chrome(options=options)
driver.get(url)
request = requests.get(url)
print(f"INFO: Page returned HTTP {request.status_code}")
request.raise_for_status()

# Wait for the page to load body
WebDriverWait(driver, 10).until(
    EC.presence_of_element_located((By.TAG_NAME, "body"))
)

# Get the fully rendered page source
page_source = driver.page_source
driver.quit()

if "<h1>404 - Not found</h1>" in page_source:
    print("ERROR: Page contains mkdocs error: '404 - Not found'")
    exit(1)