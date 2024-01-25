function doPost(e) {

    var ss = SpreadsheetApp.getActiveSpreadsheet();
    var sheet = ss.getSheets()[0]; 
    var data ;

    try { 
      data = JSON.parse(e.postData.contents);
    } 
    catch(f){
      return ContentService.createTextOutput("Error in parsing request body: " + f.message);
    }



if (data !== undefined){
    var newHeaders = Object.keys(data);

    // Retrieve the existing headers from the sheet
    var lastColumn = sheet.getLastColumn();
    var headers = lastColumn > 0 ? sheet.getRange(1, 1, 1, lastColumn).getValues()[0] : [];

    // Add 'Date' and 'Time' to the headers if needed
    if (headers.length === 0 || headers[0] !== "Date" || headers[1] !== "Time") {
        headers = ["Date", "Time"].concat(newHeaders);
        sheet.getRange(1, 1, 1, headers.length).setValues([headers]);
    }

    // Get current date and time
    var now = new Date();
    var date = Utilities.formatDate(now, Session.getScriptTimeZone(), "yyyy-MM-dd");
    var time = Utilities.formatDate(now, Session.getScriptTimeZone(), "HH:mm:ss");

    // Prepare data to be inserted, adding date and time at the beginning
    var rowData = headers.slice(2).map(function(header) { return data[header] || ''; });
    rowData = [date, time].concat(rowData);

    // Insert the new row
    sheet.appendRow(rowData);
    
    return ContentService.createTextOutput("Data successfully added to Google Sheet");
  }  else {
    return ContentService.createTextOutput("Error! Request body empty or in incorrect format.");
  }
}