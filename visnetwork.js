/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/

const { network } = require("vis-network");

class Network
{
    network = null;

    constructor( container )
    {
        try
        {
            // TODO: Do better error checking than assertions.
            console.assert( container );

            let edges = new vis.DataSet();
            let nodes = new vis.DataSet();
            let data = { nodes: nodes
                       , edges: edges };
            //  TODO: All these options should exist in a setting node, configurable by the user.
            let options = { height: '100%'
                          , width: '100%'
                          , autoResize: true
                          , layout: { hierarchical: { direction: 'LR'
                                                    // , sortMethod: 'directed' Disabled this setting, as enabling it caused all leaf nodes to become aligned.
                                                    , nodeSpacing: 50
                                                    , levelSeparation: 250
                                                    , blockShifting: true
                                                    , edgeMinimization: true
                                                    , parentCentralization: true } }
                          , physics: { enabled: false
                                     , hierarchicalRepulsion: { nodeDistance: 1 } }
                          , interaction: { dragNodes: false }
                          , edges: { smooth: { type: 'cubicBezier'
                                             , forceDirection: 'horizontal' } }
                          , nodes: { shape: 'box'
                                   , shapeProperties: { interpolation: false }
                                   , widthConstraint: { minimum: 200
                                                      , maximum: 200 }
                                   , font: { multi: 'html' } } };

            this.network = new vis.Network( container, data, options );
            
            // Presumably it's safe to register these event handlers before the event system is set up, so long as they don't fire prior to its set up.
            this.network.canvas.frame.onkeydown = network_onkeydown;
            this.network.canvas.frame.onkeyup = network_onkeyup;
            this.network.on( 'zoom', function( ev ){ kmap.event_store().fire_event( to_VectorString( [ 'subject.network', 'verb.scaled', 'object.viewport' ] ) ).throw_on_error(); } );
            this.network.on( 'deselectNode', network_on_deselect_node_event( this ) );
            this.network.on( 'selectNode', network_on_select_node_event( this ) );
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    add_edge( edge_id
            , from_id
            , to_id )
    {
        try
        {
            this.network.body.data.edges.add( { id: edge_id
                                              , from: from_id
                                              , to: to_id } );
        }
        catch( err )
        {
            console.log( String( err ) );
            throw err;
        }
    }

    // TODO: Ensure that from_ids.size() == to_ids.size()
    add_edges( edge_ids
             , from_ids
             , to_ids )
    {
        try
        {
            let data = [];
            
            for( i in from_ids )
            {
                data.push( { id: edge_ids[ i ]
                           , from: from_ids[ i ]
                           , label: to_ids[ i ] } );
            }

            this.network.body.data.edges.add( data );
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    center_viewport_node( node_id )
    {
        try
        {
            this.redraw();

            console.assert( this.network );

            let pos = this.network.getPositions( [ node_id ] )[ node_id ];

            console.assert( pos );

            this.network.moveTo( { position: { x: pos.x
                                             , y: pos.y } } );
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    change_node_font( uuid
                    , new_face
                    , new_color )
    {
        try
        {
            let n = this.network.body.data.nodes.get( uuid );

            n.font = { color: new_color
                     , face: new_face };

            this.network.body.data.nodes.update( n );
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    children_ids( node_id )
    {
        try
        {
            return this.network.getConnectedNodes( node_id, 'to' );
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    color_node_background( uuid
                         , color )
    {
        try
        {
            let n = this.network.body.data.nodes.get( uuid );

            n.color = { background: color
                      , highlight: { background: color } };

            this.network.body.data.nodes.update( n );
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    color_node_border( uuid
                     , color )
    {
        try
        {
            let n = this.network.body.data.nodes.get( uuid );

            n.color = { border: color
                      , background: '#f0f0f0' 
                      , highlight: { border: color
                                   , background: '#f0f0f0' } };

            this.network.body.data.nodes.update( n );
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    // TODO: Ensure that from_ids.size() == to_ids.size()
    create_edges( edge_ids
                , from_ids
                , to_ids )
    {
        try
        {
            let data = [];
            
            for( i in from_ids )
            {
                data.push( { id: edge_ids[ i ]
                           , from: from_ids[ i ]
                           , to: to_ids[ i ] } );
            }

            this.network.body.data.edges.add( data );
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    create_node( uuid
               , title )
    {
        try
        {
            this.network.body.data.nodes.add( { id: uuid
                                              , label: title } );
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    create_image_node( uuid
                     , title
                     , url )
    {
        try
        {
            this.network.body.data.nodes.add( { id: uuid
                                              , label: title
                                              , shape: 'image'
                                              , image: url } );
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    // TODO: Ensure that uuids.size() == titles.size()
    create_nodes( uuids
                , titles )
    {
        try
        {
            let data = [];
            
            for( i in uuids )
            {
                data.push( { id: uuids[ i ]
                           , label: titles[ i ] } );
            }

            this.network.body.data.nodes.add( data );
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    destroy_network()
    {
        try
        {
            this.network.destroy();
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    edge_exists( edge_id )
    {
        try
        {
            return this.network.body.data.edges.get( edge_id ) !== null;
        }
        catch( err )
        {
            console.log( String( err ) );
            throw err;
        }
    }

    focus_network()
    {
        try
        {
            this.network.canvas.frame.focus();
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    id_exists( id )
    {
        try
        {
            let ids = this.network.body.data.nodes.getIds();

            return ids.indexOf( id ) != -1;
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    node_ids()
    {
        try
        {
            return this.network.body.data.nodes.getIds();
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    node_position( node_id )
    {
        try
        {
            let pos = this.network.getPositions( [ node_id ] )[ node_id ];

            return pos;
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    parent_id( node_id )
    {
        try
        {
            return this.network.getConnectedNodes( node_id, 'from' )[ 0 ];
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    redraw()
    {
        try
        {
            this.network.redraw();
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    remove_edge( edge_id )
    {
        try
        {
            this.network.body.data.edges.remove( edge_id );
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    remove_edges()
    {
        try
        {
            this.network.body.data.edges.clear();
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    remove_node( nid )
    {
        try
        {
            // TODO: Not an efficient way to remove edges (iterating over all edges), but given small number displayed typically, not much of a concern.
            this.network.body.data.edges.forEach( function( e )
            {
                if( e.from === nid
                 || e.to === nid )
                {
                    this.network.body.data.edges.remove( e.id );
                }
            }.bind(this) );

            this.network.body.data.nodes.remove( { id: nid } );
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    remove_nodes()
    {
        try
        {
            this.remove_edges();

            this.network.body.data.nodes.clear();
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    select_node( node_id )
    {
        try
        {
            this.network.setSelection( { nodes: [ node_id ]
                                       , edges: [] } );

            return true;
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }
    
    selected_node_id()
    {
        try
        {
            let ns = this.network.getSelectedNodes();

            if( ns.length != 1 )
            {
                return null;
            }

            return ns[ 0 ];
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    scale_viewport( zoom_scale )
    {
        try
        {
            this.network.moveTo( { scale: zoom_scale } );
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }
    viewport_scale()
    {
        try
        {
            return this.network.getScale();
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    title_of( id )
    {
        try
        {
            return this.network.body.data.nodes.get( id ).label;
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    update_title( id
                , title )
    {
        try
        {
            this.network.body.data.nodes.update( { id: id
                                                 , label: title } );
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }
}

function new_network( container_id )
{
    try
    {
        // TODO: Do better error checking than assertions.
        const container_id_s = kmap.uuid_to_string( container_id );
        const elem_id = document.getElementById( container_id_s );

        return new Network( elem_id );
    }
    catch( err )
    {
        console.error( String( err ) );
        throw err;
    }
}

let network_on_deselect_node_event = function( network ) 
{
    try
    {
        return function( e )
        {
            if( e.nodes.length == 0 ) // New node wasn't selected.
            {
                // So, reselect the former.
                const prev_sel = e.previousSelection.nodes[ 0 ];
                // Note: double "select node" is non-straightforward workaround. Here's what I understand to be going on:
                // - kmap.select_node() assumes that the network always has a node selected, so if it's called without
                //   visjs having a node selected, it will err - even though it proceeds to call select_node() itslef.
                network.select_node( prev_sel );
                kmap.select_node( kmap.uuid_from_string( prev_sel ).value() ).throw_on_error(); // Assumes 'prev_sel' valid UUID.
            }
        };
    }
    catch( err )
    {
        console.error( String( err ) );
        throw err;
    }
};

let network_on_select_node_event = function( network ) 
{
    try
    {
        return function( e )
        {
            if( e.nodes.length != 0 )
            {
                const node = e.nodes[ 0 ];
                // Note: double "select node" is non-straightforward workaround. Here's what I understand to be going on:
                // - kmap.select_node() assumes that the network always has a node selected, so if it's called without
                //   visjs having a node selected, it will err - even though it proceeds to call select_node() itslef.
                // network.select_node( node );
                kmap.select_node( kmap.uuid_from_string( node ).value() ).throw_on_error(); // Assumes 'node' valid UUID.
            }
        };
    }
    catch( err )
    {
        console.error( String( err ) );
        throw err;
    }
};

function network_onkeydown( e )
{
    if( e.type !== "keydown" )
    {
        const err_msg = "expected 'keydown': " + e.type;
        console.error( err_msg );
        throw err_msg;
    }

    const mnemonic = e.key.toLowerCase();
    const valid_keys =
        [ 
          'arrowdown'
        , 'arrowleft'
        , 'arrowright'
        , 'arrowup'
        , 'c'
        , 'control'
        , 'e'
        , 'esc'
        , 'g'
        , 'h'
        , 'i'
        , 'j'
        , 'k'
        , 'l'
        , 'o'
        , 'p'
        , 'shift'
        , 'v'
        ];

    if( valid_keys.includes( mnemonic ) )
    {
        kmap.event_store().fire_event( to_VectorString( [ 'subject.network', 'verb.depressed', 'object.keyboard.key.' + mnemonic ] ) ).throw_on_error();
        e.preventDefault();
    }
}

function network_onkeyup( e )
{
    if( e.type !== "keyup" )
    {
        const err_msg = "expected 'keyup': " + e.type;
        console.error( err_msg );
        throw err_msg;
    }

    const mnemonic = e.key.toLowerCase();
    const valid_keys =
        [ 
          'arrowdown'
        , 'arrowleft'
        , 'arrowright'
        , 'arrowup'
        , 'c'
        , 'control'
        , 'e'
        , 'esc'
        , 'g'
        , 'h'
        , 'i'
        , 'j'
        , 'k'
        , 'l'
        , 'o'
        , 'p'
        , 'shift'
        , 'v'
        ];

    if( valid_keys.includes( mnemonic ) )
    {
        kmap.event_store().fire_event( to_VectorString( [ 'subject.network', 'verb.raised', 'object.keyboard.key.' + mnemonic ] ) ).throw_on_error();
        e.preventDefault();
    }
}

function kmap_vnw_create_node( vnw, nid, nlabel )
{
    const nw = kmap.network();
    const img_node = kmap.fetch_descendant( kmap.uuid_from_string( nid ).value(), '.$.node.image' );

    if( img_node.has_value() )
    {
        const img_url = nw.fetch_body( img_node.value() ); img_url.throw_on_error();

        vnw.body.data.nodes.add( { id: nid 
                                 , label: nlabel
                                 , shape: 'image'
                                 , image: img_url.value() } );
    }
    else
    {
        vnw.body.data.nodes.add( { id: nid 
                                 , label: nlabel } );
    }
}

async function kmap_qsort_async( arr, comparator )
{
    if( arr.length <= 1 ) {
        return arr;
    }

    const pivot = arr[ arr.length - 1 ];
    const leftArr = [];
    const rightArr = [];

    for( let i = 0; i < arr.length - 1; i++ ) {
        const isLeftSmaller = await comparator( arr[ i ], pivot );
        
        if( isLeftSmaller ) {
            console.log( 'left smaller' );
            leftArr.push( arr[ i ] );
        } else {
            console.log( 'right smaller' );
            rightArr.push( arr[ i ] );
        }
    }

    return [
        ...( await kmap_qsort_async( leftArr, comparator ) ),
        pivot,
        ...( await kmap_qsort_async( rightArr, comparator ) )
    ];
}

async function kmap_qsort_prompt_comparator( nw )
{
    return (async function( lhs, rhs ) {
        return new Promise( ( resolve, reject ) => {
            console.log( `comparing ${lhs} <? ${rhs}` )
            if( lhs == rhs) {
                nw.body.data.nodes.clear();
                nw.body.data.edges.clear();
                resolve( false ); 
                return false ;
            }
            nw.on( 'selectNode', function ( e ) {
                if( e.nodes.length != 0 )
                {
                    const node = e.nodes[ 0 ];
                    console.log( 'select node: ' + node ); 
                    nw.body.data.nodes.clear();
                    nw.body.data.edges.clear();
                    if( node == lhs )
                    {
                        resolve( true );
                    }
                    else
                    {
                        resolve( false ); 
                    }
                }
                else
                {
                    reject( 'no node selected: ' + node );
                }
        } );
        const lhs_uuid = kmap.uuid_from_string( lhs ); lhs_uuid.throw_on_error();
        const rhs_uuid = kmap.uuid_from_string( rhs ); rhs_uuid.throw_on_error();
        const lhs_label = kmap.format_node_label( lhs_uuid.value() ); lhs_label.throw_on_error();
        const rhs_label = kmap.format_node_label( rhs_uuid.value() ); rhs_label.throw_on_error();
        kmap_vnw_create_node( nw, lhs, lhs_label.value() );
        kmap_vnw_create_node( nw, rhs, rhs_label.value() );
        nw.body.data.edges.add( { id: lhs + rhs, from: lhs, to: rhs } );
        // Center:
        const lpos = nw.getPositions( [ lhs ] )[ lhs ];
        const rpos = nw.getPositions( [ rhs ] )[ rhs ];
        console.assert( lpos );
        console.assert( rpos );
        nw.moveTo( { position: { x: ( lpos.x + rpos.x ) / 2
                               , y: ( lpos.y + rpos.y ) / 2 }
                   ,  scale: 4.00 } ); // TODO: Should come from an option.
        } );
    } );
}

async function kmap_sort_children_prompt( node )
{
    let nw = null;
    const dialog = vexjs.open({
        unsafeContent: '<div id="kmap.nw.cmp.prompt"></div>',
        afterOpen: function () {
        }
    });
    dialog.contentEl.style.width = '1400px'; // TODO: Can I auto calculate this to be 2/3 of window?
    dialog.contentEl.style.height = '800px';
    // Note: Placing nw creation after dialog creation so that first render will get the adjusted window dimensions.
    const container = document.getElementById( 'kmap.nw.cmp.prompt' );
    console.log( container );
    container.style.width = '1400px'; // TODO: Can I auto calculate this to be 2/3 of window?
    container.style.height = '800px'; // TODO: Can I auto calculate this to be 2/3 of window?
    let edges = new vis.DataSet();
    let nodes = new vis.DataSet();
    let data = {
        nodes: nodes,
        edges: edges
    };
    //  TODO: All these options should exist in a setting node, configurable by the user.
    //        meta.option.sort.prompt.<option>
    let options = {
        height: '100%'
        , width: '100%'
        , autoResize: true
        , layout: {
            hierarchical: {
                direction: 'LR'
                // , sortMethod: 'directed' Disabled this setting, as enabling it caused all leaf nodes to become aligned.
                // , nodeSpacing: 50
                // , levelSeparation: 250
                , blockShifting: true
                , edgeMinimization: true
                , parentCentralization: true
            }
        }
        , physics: {
            enabled: false
            , hierarchicalRepulsion: { nodeDistance: 1 }
        }
        , interaction: { dragNodes: false }
        , edges: {
            smooth: {
                type: 'cubicBezier'
                , forceDirection: 'horizontal'
            }
        }
        , nodes: {
            shape: 'box'
            , shapeProperties: { interpolation: false }
            , widthConstraint: {
                minimum: 100
                , maximum: 100
            }
            , font: { multi: 'html' }
        }
    };

    nw = new vis.Network(container, data, options);

    nw.canvas.frame.onkeydown = function () { /*do nothing.*/ };
    nw.canvas.frame.onkeyup = function () {
        console.log( 'key downed' );
    };
    
    // Note: I don't know why, but lifetime of "node/Uuid" input param expires during execution,
    //       but converting it internal to the function (converting from string and back to type hack to create copy), works.
    //       Probably an issue with async functions.
    const parent_s = kmap.uuid_to_string( node );
    const parent = kmap.uuid_from_string( parent_s ).value();
    const children = kmap.fetch_children( parent );
    let cids = []
    for( i = 0; i < children.size(); ++i ){ cids.push( kmap.uuid_to_string( children.get( i ) ) ); } // Convert vector<Uuid> to JS list/array.
    const qsr = await kmap_qsort_async( cids, await kmap_qsort_prompt_comparator( nw ) );
    dialog.close();

    if( qsr.length > 1 )
    {
        let knw = kmap.network();
        let sorted_ids = new kmap.std$$vector$Uuid$();
        for( i = 0; i < qsr.length; ++i ){ sorted_ids.push_back( kmap.uuid_from_string( qsr[ i ] ).value() ); } // Convert vector<Uuid> to JS list/array.
        knw.reorder_children( parent, sorted_ids );
        knw.select_node( parent ).throw_on_error();
    }
}

async function kmap_sort_children_prompt( node )
{
    let nw = null;
    const dialog = vexjs.open({
        unsafeContent: '<div id="kmap.nw.cmp.prompt"></div>',
        afterOpen: function () {
        }
    });
    dialog.contentEl.style.width = '1400px'; // TODO: Can I auto calculate this to be 2/3 of window?
    dialog.contentEl.style.height = '800px';
    // Note: Placing nw creation after dialog creation so that first render will get the adjusted window dimensions.
    const container = document.getElementById( 'kmap.nw.cmp.prompt' );
    console.log( container );
    container.style.width = '1400px'; // TODO: Can I auto calculate this to be 2/3 of window?
    container.style.height = '800px'; // TODO: Can I auto calculate this to be 2/3 of window?
    let edges = new vis.DataSet();
    let nodes = new vis.DataSet();
    let data = {
        nodes: nodes,
        edges: edges
    };
    //  TODO: All these options should exist in a setting node, configurable by the user.
    //        meta.option.sort.prompt.<option>
    let options = {
        height: '100%'
        , width: '100%'
        , autoResize: true
        , layout: {
            hierarchical: {
                direction: 'LR'
                // , sortMethod: 'directed' Disabled this setting, as enabling it caused all leaf nodes to become aligned.
                // , nodeSpacing: 50
                // , levelSeparation: 250
                , blockShifting: true
                , edgeMinimization: true
                , parentCentralization: true
            }
        }
        , physics: {
            enabled: false
            , hierarchicalRepulsion: { nodeDistance: 1 }
        }
        , interaction: { dragNodes: false }
        , edges: {
            smooth: {
                type: 'cubicBezier'
                , forceDirection: 'horizontal'
            }
        }
        , nodes: {
            shape: 'box'
            , shapeProperties: { interpolation: false }
            , widthConstraint: {
                minimum: 100
                , maximum: 100
            }
            , font: { multi: 'html' }
        }
    };

    nw = new vis.Network(container, data, options);

    nw.canvas.frame.onkeydown = function () { /*do nothing.*/ };
    nw.canvas.frame.onkeyup = function () {
        console.log( 'key downed' );
    };
    
    // Note: I don't know why, but lifetime of "node/Uuid" input param expires during execution,
    //       but converting it internal to the function (converting from string and back to type hack to create copy), works.
    //       Probably an issue with async functions.
    const parent_s = kmap.uuid_to_string( node );
    const parent = kmap.uuid_from_string( parent_s ).value();
    const children = kmap.fetch_children( parent );
    let cids = []
    for( i = 0; i < children.size(); ++i ){ cids.push( kmap.uuid_to_string( children.get( i ) ) ); } // Convert vector<Uuid> to JS list/array.
    const qsr = await kmap_qsort_async( cids, await kmap_qsort_prompt_comparator( nw ) );
    dialog.close();

    if( qsr.length > 1 )
    {
        let knw = kmap.network();
        let sorted_ids = new kmap.std$$vector$Uuid$();
        for( i = 0; i < qsr.length; ++i ){ sorted_ids.push_back( kmap.uuid_from_string( qsr[ i ] ).value() ); } // Convert vector<Uuid> to JS list/array.
        knw.reorder_children( parent, sorted_ids );
        knw.select_node( parent ).throw_on_error();
    }
}

async function kmap_insert_child_prompt( parent, child )
{
    // TODO: This won't be qsort, but similarly based, binary descent.
    //       From siblings, split in half, larger or smaller? OK, next set: larger or smaller?
    let nw = null;
    const dialog = vexjs.open({
        unsafeContent: '<div id="kmap.nw.cmp.prompt"></div>',
        afterOpen: function () {
        }
    });
    dialog.contentEl.style.width = '1400px'; // TODO: Can I auto calculate this to be 2/3 of window?
    dialog.contentEl.style.height = '800px';
    // Note: Placing nw creation after dialog creation so that first render will get the adjusted window dimensions.
    const container = document.getElementById( 'kmap.nw.cmp.prompt' );
    console.log( container )
    container.style.width = '1400px'; // TODO: Can I auto calculate this to be 2/3 of window?
    container.style.height = '800px'; // TODO: Can I auto calculate this to be 2/3 of window?
    let edges = new vis.DataSet();
    let nodes = new vis.DataSet();
    let data = {
        nodes: nodes
        , edges: edges
    };
    //  TODO: All these options should exist in a setting node, configurable by the user.
    //        meta.option.sort.prompt.<option>
    let options = {
        height: '100%'
        , width: '100%'
        , autoResize: true
        , layout: {
            hierarchical: {
                direction: 'LR'
                // , sortMethod: 'directed' Disabled this setting, as enabling it caused all leaf nodes to become aligned.
                // , nodeSpacing: 50
                // , levelSeparation: 250
                , blockShifting: true
                , edgeMinimization: true
                , parentCentralization: true
            }
        }
        , physics: {
            enabled: false
            , hierarchicalRepulsion: { nodeDistance: 1 }
        }
        , interaction: { dragNodes: false }
        , edges: {
            smooth: {
                type: 'cubicBezier'
                , forceDirection: 'horizontal'
            }
        }
        , nodes: {
            shape: 'box'
            , shapeProperties: { interpolation: false }
            , widthConstraint: {
                minimum: 100
                , maximum: 100
            }
            , font: { multi: 'html' }
        }
    };

    nw = new vis.Network(container, data, options);

    nw.canvas.frame.onkeydown = function () { /*do nothing.*/ };
    nw.canvas.frame.onkeyup = function () {
        console.log( 'key downed' );
    };
    
    const selected = kmap.selected_node();
    const children = kmap.fetch_children( selected );
    let cids = []
    for( i = 0; i < children.size(); ++i ){ cids.push( kmap.uuid_to_string( children.get( i ) ) ); } // Convert vector<Uuid> to JS list/array.
    const qsr = await kmap_qsort_async( cids, await kmap_qsort_prompt_comparator( nw ) );
    dialog.close();

    if( qsr.length > 1 )
    {
        let knw = kmap.network();
        let sorted_ids = new kmap.std$$vector$Uuid$();
        for( i = 0; i < qsr.length; ++i ){ sorted_ids.push_back( kmap.uuid_from_string( qsr[ i ] ).value() ); } // Convert vector<Uuid> to JS list/array.
        knw.reorder_children( selected, sorted_ids );
        knw.select_node( selected ); // TODO: Throw on error?
    }
}
