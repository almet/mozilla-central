function saChangeDirectoryByDOMNode(dirNode)
{
	var uri = dirNode.getAttribute('id');
	dump(uri + "\n");
	saChangeDirectoryByURI(uri);
}

function saChangeDirectoryByURI(uri)
{
	var tree = frames["browser.selAddrResultPane"].document.getElementById('resultTree');
	//dump("tree = " + tree + "\n");

	var treechildrenList = tree.getElementsByTagName('treechildren');
	if ( treechildrenList.length == 1 )
	{
		var body = treechildrenList[0];
		body.setAttribute('id', uri);// body no longer valid after setting id.
	}
}


function SelectAddressToButton()
{
	AddSelectedAddressesIntoBucket("To: ");
}

function SelectAddressCcButton()
{
	AddSelectedAddressesIntoBucket("Cc: ");
}

function SelectAddressBccButton()
{
	AddSelectedAddressesIntoBucket("Bcc: ");
}

function SelectAddressOKButton()
{
	top.window.close();
}

function SelectAddressCancelButton()
{
	top.window.close();
}


function SelectAddressNewButton()
{
	AbNewCard();
}

function SelectAddressEditButton()
{
	var rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
	rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);

	var resultsDoc = frames["browser.selAddrResultPane"].document;
	var selArray = resultsDoc.getElementsByAttribute('selected', 'true');

	if ( selArray && selArray.length == 1 )
	{
		var uri = selArray[0].getAttribute('id');
		var card = rdf.GetResource(uri);
		card = card.QueryInterface(Components.interfaces.nsIAbCard);
		AbEditCard(card);
	}
}

function AddSelectedAddressesIntoBucket(prefix)
{
	var item, uri, rdf, cardResource, card, address;
	var resultsDoc = frames["browser.selAddrResultPane"].document;
	var bucketDoc = frames["browser.addressbucket"].document;
	
	rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
	rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);

	var selArray = resultsDoc.getElementsByAttribute('selected', 'true');
	if ( selArray && selArray.length )
	{
		for ( item = 0; item < selArray.length; item++ )
		{
			uri = selArray[item].getAttribute('id');
			cardResource = rdf.GetResource(uri);
			card = cardResource.QueryInterface(Components.interfaces.nsIAbCard);
			address = prefix + "\"" + card.DisplayName + "\" <" + card.PrimaryEmail + ">";
			AddAddressIntoBucket(bucketDoc, address);
		}
	}	
}

function AddAddressIntoBucket(doc, address)
{
	var body = doc.getElementById("bucketBody");
	
	var item = doc.createElement('treeitem');
	var row = doc.createElement('treerow');
	var cell = doc.createElement('treecell');
	var text = doc.createTextNode(address);
	
	cell.appendChild(text);
	row.appendChild(cell);
	item.appendChild(row);
	body.appendChild(item);
}

function RemoveSelectedFromBucket()
{
	var bucketDoc = frames["browser.addressbucket"].document;
	var body = bucketDoc.getElementById("bucketBody");
	
	var selArray = body.getElementsByAttribute('selected', 'true');
	if ( selArray && selArray.length )
	{
		for ( var item = selArray.length - 1; item >= 0; item-- )
			body.removeChild(selArray[item]);
	}	
}
